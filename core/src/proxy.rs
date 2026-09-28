//! Local filtering proxy — the second blocking layer.
//!
//! WebKitGTK's network session is pointed at this loopback proxy. Plain
//! HTTP requests arrive with full URLs; HTTPS arrives as CONNECT tunnels.
//! We run host-level (and URL-level for plain HTTP) matching through the
//! Rust filter engine and refuse to dial out for blocked hosts.
//! **No TLS interception**: CONNECT tunnels are byte-for-byte relays, so
//! certificates remain end-to-end. Blocks are counted per blocked host and
//! surfaced in the privacy dashboard.

use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream, ToSocketAddrs};
use std::sync::atomic::{AtomicBool, Ordering};

static ENABLED: AtomicBool = AtomicBool::new(true);

pub fn set_enabled(on: bool) {
    ENABLED.store(on, Ordering::SeqCst);
}

pub fn start() -> std::io::Result<u16> {
    let listener = TcpListener::bind(("127.0.0.1", 0))?;
    let port = listener.local_addr()?.port();
    std::thread::spawn(move || {
        for stream in listener.incoming() {
            match stream {
                Ok(s) => {
                    std::thread::spawn(move || {
                        if let Err(e) = serve(s) {
                            let _ = writeln!(std::io::stderr(), "wed-proxy: {e}");
                        }
                    });
                }
                Err(_) => continue,
            }
        }
    });
    Ok(port)
}

fn decide_url(url: &str) -> i32 {
    if !ENABLED.load(Ordering::Relaxed) {
        return 0;
    }
    let core = crate::Core::global();
    let mut engine = core.engine.lock().unwrap();
    match engine.check(url, "", 0) {
        crate::filters::Decision::Allow => 0,
        crate::filters::Decision::Block(crate::filters::Category::Tracker) => {
            core.blocked_trackers.fetch_add(1, Ordering::Relaxed);
            record_stat(url, false);
            2
        }
        crate::filters::Decision::Block(_) => {
            core.blocked_ads.fetch_add(1, Ordering::Relaxed);
            record_stat(url, true);
            1
        }
    }
}

fn record_stat(url: &str, is_ad: bool) {
    let host = crate::filters::host_of(url).to_string();
    if !host.is_empty() {
        crate::Core::global().db.lock().unwrap().stat_blocked(&host, is_ad);
    }
}

fn serve(mut client: TcpStream) -> std::io::Result<()> {
    client.set_nodelay(true).ok();
    let mut buf = [0u8; 8192];
    let n = client.read(&mut buf)?;
    if n == 0 {
        return Ok(());
    }
    let head = String::from_utf8_lossy(&buf[..n]);
    let first_line = head.lines().next().unwrap_or("").to_string();

    if let Some(rest) = first_line.strip_prefix("CONNECT ") {
        // rest is "host:port HTTP/1.1"
        let hostport = rest.split_whitespace().next().unwrap_or(rest).to_string();
        return handle_connect(client, &hostport, &buf[n.min(n)..n]);
    }

    if first_line.starts_with("GET ") || first_line.starts_with("POST ")
        || first_line.starts_with("HEAD ") || first_line.starts_with("PUT ")
        || first_line.starts_with("DELETE ") || first_line.starts_with("OPTIONS ")
        || first_line.starts_with("PATCH ")
    {
        return handle_plain(client, &first_line, head.to_string(), &buf[..n]);
    }
    Ok(())
}

/// CONNECT host:port — check host, then tunnel.
fn handle_connect(mut client: TcpStream, hostport: &str, _extra: &[u8]) -> std::io::Result<()> {
    // hostport is "host:port"; build a check URL with just the host
    let host = hostport.split(':').next().unwrap_or(hostport);
    let url = format!("https://{host}/");
    let verdict = decide_url(&url);
    if verdict != 0 {
        let _ = client.write_all(b"HTTP/1.1 403 Blocked by WED\r\nContent-Length: 0\r\n\r\n");
        return Ok(());
    }
    let mut remote = connect_host(hostport)?;
    let _ = client.write_all(b"HTTP/1.1 200 Connection Established\r\n\r\n");
    tunnel(client, remote)
}

/// Plain-HTTP proxy request — full URL is visible; rewrite to origin-form.
fn handle_plain(mut client: TcpStream, first_line: &str, full_head: String, raw: &[u8]) -> std::io::Result<()> {
    // GET http://host/path HTTP/1.1
    let parts: Vec<&str> = first_line.split(' ').collect();
    if parts.len() < 2 {
        return Ok(());
    }
    let abs = parts[1];
    let verdict = decide_url(abs);
    if verdict != 0 {
        let _ = client.write_all(b"HTTP/1.1 403 Blocked by WED\r\nContent-Length: 0\r\n\r\n");
        return Ok(());
    }
    let (host, port, path) = match split_absolute(abs) {
        Some(v) => v,
        None => return Ok(()),
    };
    let hostport = format!("{host}:{port}");
    let mut remote = connect_host(&hostport)?;

    // rewrite request line to origin form; pass headers onward
    let rest_of_head = full_head.lines().skip(1).collect::<Vec<_>>().join("\r\n");
    let new_head = format!("{} {} HTTP/1.1\r\n{}\r\n", parts[0], path, rest_of_head);
    let body_start = raw.len() - raw.len(); // body bytes beyond head already in raw
    let head_len = full_head.find("\r\n\r\n").map(|i| i + 4).unwrap_or(raw.len());
    let mut out = new_head.into_bytes();
    out.extend_from_slice(&raw[head_len.min(raw.len())..]);
    let _ = body_start;
    remote.write_all(&out)?;
    tunnel(client, remote)
}

fn tunnel(client: TcpStream, remote: TcpStream) -> std::io::Result<()> {
    // each stream is read AND written from two directions: clone for one side
    let mut c1 = client.try_clone()?;
    let mut r1 = remote.try_clone()?;
    let t1 = std::thread::spawn(move || relay(&mut c1, &mut r1));
    let mut c2 = client;
    let mut r2 = remote;
    let t2 = std::thread::spawn(move || relay(&mut r2, &mut c2));
    let _ = t1.join();
    let _ = t2.join();
    Ok(())
}

fn relay(from: &mut TcpStream, to: &mut TcpStream) {
    let mut buf = [0u8; 16384];
    loop {
        match from.read(&mut buf) {
            Ok(0) | Err(_) => break,
            Ok(n) => {
                if to.write_all(&buf[..n]).is_err() {
                    break;
                }
            }
        }
    }
    // half-close best effort
    let _ = to.shutdown(std::net::Shutdown::Write);
}

fn connect_host(hostport: &str) -> std::io::Result<TcpStream> {
    let addrs: Vec<_> = hostport.to_socket_addrs()?.collect();
    let mut last_err = None;
    for a in addrs {
        match TcpStream::connect(a) {
            Ok(s) => return Ok(s),
            Err(e) => last_err = Some(e),
        }
    }
    Err(last_err.unwrap_or_else(|| std::io::Error::new(std::io::ErrorKind::Other, "no address")))
}

/// "http://host:port/path?q" → (host, port, "/path?q")
fn split_absolute(abs: &str) -> Option<(String, u16, String)> {
    let rest = abs.strip_prefix("http://")?;
    let (authority, path) = match rest.find('/') {
        Some(i) => (&rest[..i], &rest[i..]),
        None => (rest, "/"),
    };
    let (host, port) = match authority.rsplit_once(':') {
        Some((h, p)) if p.chars().all(|c| c.is_ascii_digit()) => (h.to_string(), p.parse().unwrap_or(80)),
        _ => (authority.to_string(), 80),
    };
    Some((host, port, path.to_string()))
}
