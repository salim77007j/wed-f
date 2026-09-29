//! SQLite data layer: history, bookmarks, downloads, sessions, settings,
//! per-site shield/permission exceptions, and per-host block statistics.
//! Single connection behind a Mutex; WAL journal; all public fns expect
//! the caller to hold the lock.

use rusqlite::{params, Connection, OptionalExtension, Row};
use std::path::PathBuf;
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

fn now() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0)
}

pub struct Database {
    conn: Option<Connection>,
}

impl Database {
    pub fn open_default() -> Database {
        Database { conn: None }
    }

    pub fn open(&mut self, data_dir: &str) -> Result<(), rusqlite::Error> {
        let mut path = PathBuf::from(if data_dir.is_empty() { ".".into() } else { data_dir.to_string() });
        std::fs::create_dir_all(&path).ok();
        path.push("wed.db");
        let conn = Connection::open(&path)?;
        conn.pragma_update(None, "journal_mode", "WAL")?;
        conn.pragma_update(None, "synchronous", "NORMAL")?;
        conn.execute_batch(
            "CREATE TABLE IF NOT EXISTS history(
                url TEXT PRIMARY KEY, title TEXT NOT NULL DEFAULT '',
                ts INTEGER NOT NULL, visits INTEGER NOT NULL DEFAULT 1);
             CREATE INDEX IF NOT EXISTS history_ts ON history(ts DESC);
             CREATE INDEX IF NOT EXISTS history_visits ON history(visits DESC);
             CREATE TABLE IF NOT EXISTS bookmarks(
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                url TEXT UNIQUE NOT NULL, title TEXT NOT NULL DEFAULT '',
                folder TEXT NOT NULL DEFAULT 'root', ts INTEGER NOT NULL);
             CREATE TABLE IF NOT EXISTS downloads(
                url TEXT NOT NULL, path TEXT NOT NULL, size INTEGER NOT NULL,
                ok INTEGER NOT NULL, ts INTEGER NOT NULL);
             CREATE TABLE IF NOT EXISTS session(
                id INTEGER PRIMARY KEY CHECK (id = 1), tabs TEXT, window TEXT, ts INTEGER);
             CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL);
             CREATE TABLE IF NOT EXISTS site_settings(
                host TEXT NOT NULL, key TEXT NOT NULL, value TEXT NOT NULL,
                PRIMARY KEY (host, key));
             CREATE TABLE IF NOT EXISTS site_stats(
                host TEXT PRIMARY KEY, ads INTEGER NOT NULL DEFAULT 0,
                trackers INTEGER NOT NULL DEFAULT 0, last INTEGER NOT NULL DEFAULT 0);
             CREATE TABLE IF NOT EXISTS top_sites_cache(ts INTEGER);"
        )?;
        self.conn = Some(conn);
        Ok(())
    }

    fn with<T>(&self, f: impl FnOnce(&Connection) -> T) -> T {
        // Called under external lock; open a guaranteed connection if missing.
        match &self.conn {
            Some(c) => f(c),
            None => f(&Connection::open_in_memory().expect("wed sqlite memory")),
        }
    }

    // ------------------------------------------------------------ history

    pub fn history_add(&self, url: &str, title: &str) {
        if url.is_empty() {
            return;
        }
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO history(url,title,ts,visits) VALUES(?1,?2,?3,1)
                 ON CONFLICT(url) DO UPDATE SET
                   ts=?3, visits=visits+1,
                   title=CASE WHEN ?2 != '' THEN ?2 ELSE title END",
                params![url, title, now()],
            );
        });
    }

    pub fn history_json(&self, limit: i32, query: &str) -> String {
        let rows: Vec<(String, String, i64, i64)> = self.with(|c| {
            let like = format!("%{}%", query.replace('%', ""));
            let mut stmt = c.prepare(
                "SELECT url,title,ts,visits FROM history
                 WHERE (?2 = '' OR url LIKE ?3 OR title LIKE ?3)
                 ORDER BY ts DESC LIMIT ?1",
            ).expect("history query");
            let r = stmt
                .query_map(params![limit, query, like], row4)
                .expect("history rows")
                .filter_map(|r| r.ok())
                .collect();
            r
        });
        format!("[{}]", rows.iter().map(|(u, t, ts, v)| {
            format!("{{\"url\":{},\"title\":{},\"ts\":{},\"visits\":{}}}",
                    jq(u), jq(t), ts, v)
        }).collect::<Vec<_>>().join(","))
    }

    pub fn history_clear(&self) {
        self.with(|c| {
            let _ = c.execute("DELETE FROM history", []);
            let _ = c.execute("DELETE FROM site_stats", []);
        });
    }

    pub fn history_remove(&self, url: &str) {
        self.with(|c| {
            let _ = c.execute("DELETE FROM history WHERE url = ?1", params![url]);
        });
    }

    /// Speed dial: top sites by visits (title non-empty preferred).
    pub fn top_sites_json(&self) -> String {
        let rows: Vec<(String, String)> = self.with(|c| {
            let mut stmt = c.prepare(
                "SELECT url, title FROM history
                 WHERE visits >= 1 AND url LIKE 'http%'
                 ORDER BY visits * 1000000 - (strftime('%s','now') - ts) DESC
                 LIMIT 12",
            ).expect("top sites");
            let r = stmt.query_map([], |row| Ok((row.get(0)?, row.get(1)?)))
                .expect("top sites rows").filter_map(|r| r.ok()).collect();
            r
        });
        format!("[{}]", rows.iter().map(|(u, t)| {
            format!("{{\"url\":{},\"title\":{}}}", jq(u), jq(t))
        }).collect::<Vec<_>>().join(","))
    }

    // ------------------------------------------------------------ bookmarks

    pub fn bookmark_add(&self, url: &str, title: &str, folder: &str) -> i64 {
        self.with(|c| {
            match c.execute(
                "INSERT OR REPLACE INTO bookmarks(url,title,folder,ts) VALUES(?1,?2,?3,?4)",
                params![url, title, folder, now()],
            ) {
                Ok(_) => c.last_insert_rowid(),
                Err(_) => -1,
            }
        })
    }

    pub fn bookmark_remove(&self, url: &str) {
        self.with(|c| {
            let _ = c.execute("DELETE FROM bookmarks WHERE url = ?1", params![url]);
        });
    }

    pub fn bookmarks_json(&self, folder: &str) -> String {
        let rows: Vec<(String, String)> = self.with(|c| {
            let mut stmt = c.prepare(
                "SELECT url,title FROM bookmarks WHERE folder = ?1 ORDER BY ts DESC",
            ).expect("bookmarks query");
            let r = stmt.query_map(params![folder], |row| Ok((row.get(0)?, row.get(1)?)))
                .expect("bookmark rows").filter_map(|r| r.ok()).collect();
            r
        });
        format!("[{}]", rows.iter().map(|(u, t)| {
            format!("{{\"url\":{},\"title\":{}}}", jq(u), jq(t))
        }).collect::<Vec<_>>().join(","))
    }

    pub fn is_bookmarked(&self, url: &str) -> bool {
        self.with(|c| {
            c.query_row("SELECT 1 FROM bookmarks WHERE url = ?1", params![url], |_| Ok(()))
                .optional().unwrap_or(None).is_some()
        })
    }

    // ------------------------------------------------------------ sessions

    pub fn session_save(&self, tabs: &str, window: &str) {
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO session(id,tabs,window,ts) VALUES(1,?1,?2,?3)
                 ON CONFLICT(id) DO UPDATE SET tabs=?1, window=?2, ts=?3",
                params![tabs, window, now()],
            );
        });
    }

    pub fn session_load(&self) -> Option<String> {
        self.with(|c| {
            c.query_row("SELECT tabs FROM session WHERE id = 1", [],
                       |r| r.get::<_, String>(0)).optional().unwrap_or(None)
        })
    }

    pub fn session_window_state(&self) -> Option<String> {
        self.with(|c| {
            c.query_row("SELECT window FROM session WHERE id = 1", [],
                       |r| r.get::<_, String>(0)).optional().unwrap_or(None)
        })
    }

    // ------------------------------------------------------------ settings

    pub fn settings_get(&self, key: &str, default: &str) -> String {
        self.with(|c| {
            c.query_row("SELECT value FROM settings WHERE key = ?1", params![key],
                        |r| r.get::<_, String>(0))
                .optional().unwrap_or(None).unwrap_or_else(|| default.to_string())
        })
    }

    pub fn settings_set(&self, key: &str, value: &str) {
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO settings(key,value) VALUES(?1,?2)
                 ON CONFLICT(key) DO UPDATE SET value=?2",
                params![key, value],
            );
        });
    }

    // ------------------------------------------------------------ shields

    pub fn shields_enabled(&self, host: &str) -> bool {
        let v = self.with(|c| {
            c.query_row("SELECT value FROM site_settings WHERE host=?1 AND key='shields'",
                        params![host], |r| r.get::<_, String>(0))
                .optional().unwrap_or(None)
        });
        v.map(|v| v == "on").unwrap_or(true)
    }

    pub fn set_shields(&self, host: &str, on: bool) {
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO site_settings(host,key,value) VALUES(?1,'shields',?2)
                 ON CONFLICT(host,key) DO UPDATE SET value=?2",
                params![host, if on { "on" } else { "off" }],
            );
        });
    }

    pub fn permission_set(&self, host: &str, kind: &str, value: &str) {
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO site_settings(host,key,value) VALUES(?1,?2,?3)
                 ON CONFLICT(host,key) DO UPDATE SET value=?3",
                params![host, format!("perm.{kind}"), value],
            );
        });
    }

    pub fn permission_get(&self, host: &str, kind: &str) -> String {
        self.with(|c| {
            c.query_row("SELECT value FROM site_settings WHERE host=?1 AND key=?2",
                        params![host, format!("perm.{kind}")], |r| r.get::<_, String>(0))
                .optional().unwrap_or(None).unwrap_or_else(|| "ask".into())
        })
    }

    pub fn site_settings_json(&self) -> String {
        let rows: Vec<(String, String, String)> = self.with(|c| {
            let mut stmt = c.prepare(
                "SELECT host,key,value FROM site_settings ORDER BY host, key",
            ).expect("site settings query");
            let r = stmt.query_map([], row3).expect("site settings rows")
                .filter_map(|r| r.ok()).collect();
            r
        });
        format!("[{}]", rows.iter().map(|(h, k, v)| {
            format!("{{\"host\":{},\"key\":{},\"value\":{}}}", jq(h), jq(k), jq(v))
        }).collect::<Vec<_>>().join(","))
    }

    pub fn site_reset(&self, host: &str) {
        self.with(|c| {
            let _ = c.execute("DELETE FROM site_settings WHERE host = ?1", params![host]);
        });
    }

    // ------------------------------------------------------------ stats

    /// Persistent lifetime counters (Chrome-parity: survive restarts).
    pub fn gstat_bump(&self, key: &str, inc: u64) {
        self.with(|c| {
            let _ = c.execute(
                "CREATE TABLE IF NOT EXISTS global_stats(key TEXT PRIMARY KEY, value INTEGER NOT NULL DEFAULT 0)",
                [],
            );
            let _ = c.execute(
                "INSERT INTO global_stats(key, value) VALUES(?1, ?2)
                 ON CONFLICT(key) DO UPDATE SET value = value + ?2",
                params![key, inc],
            );
        });
    }

    pub fn gstat_get(&self, key: &str) -> u64 {
        self.with(|c| {
            let _ = c.execute(
                "CREATE TABLE IF NOT EXISTS global_stats(key TEXT PRIMARY KEY, value INTEGER NOT NULL DEFAULT 0)",
                [],
            );
            c.query_row(
                "SELECT value FROM global_stats WHERE key = ?1",
                params![key],
                |r| r.get::<_, i64>(0),
            ).unwrap_or(0) as u64
        })
    }

    pub fn stat_blocked(&self, blocked_host: &str, is_ad: bool) {
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO site_stats(host,ads,trackers,last) VALUES(?1,?2,?3,?4)
                 ON CONFLICT(host) DO UPDATE SET
                   ads = ads + ?2, trackers = trackers + ?3, last = ?4",
                params![blocked_host, if is_ad { 1 } else { 0 }, if is_ad { 0 } else { 1 }, now()],
            );
        });
    }

    pub fn site_stats_json(&self) -> String {
        let rows: Vec<(String, i64, i64, i64)> = self.with(|c| {
            let mut stmt = c.prepare(
                "SELECT host,ads,trackers,last FROM site_stats
                 ORDER BY (ads+trackers) DESC LIMIT 200",
            ).expect("stats query");
            let r = stmt.query_map([], |row| {
                Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?,
                    row.get::<_, i64>(2)?, row.get::<_, i64>(3)?))
            }).expect("stats rows").filter_map(|r| r.ok()).collect();
            r
        });
        format!("[{}]", rows.iter().map(|(h, a, t, l)| {
            format!("{{\"host\":{},\"ads\":{},\"trackers\":{},\"last\":{}}}", jq(h), a, t, l)
        }).collect::<Vec<_>>().join(","))
    }

    // ------------------------------------------------------------ downloads

    pub fn download_record(&self, url: &str, path: &str, size: i64, ok: bool) {
        self.with(|c| {
            let _ = c.execute(
                "INSERT INTO downloads(url,path,size,ok,ts) VALUES(?1,?2,?3,?4,?5)",
                params![url, path, size, ok as i64, now()],
            );
        });
    }

    pub fn downloads_json(&self) -> String {
        let rows: Vec<(String, String, i64, i64, i64)> = self.with(|c| {
            let mut stmt = c.prepare(
                "SELECT url,path,size,ok,ts FROM downloads ORDER BY ts DESC LIMIT 200",
            ).expect("downloads query");
            let r = stmt.query_map([], row5).expect("download rows")
                .filter_map(|r| r.ok()).collect();
            r
        });
        format!("[{}]", rows.iter().map(|(u, p, s, ok, ts)| {
            format!("{{\"url\":{},\"path\":{},\"size\":{},\"ok\":{},\"ts\":{}}}",
                    jq(u), jq(p), s, ok, ts)
        }).collect::<Vec<_>>().join(","))
    }
}

// ------------------------------------------------------------------ helpers

fn row3(r: &Row) -> rusqlite::Result<(String, String, String)> {
    Ok((r.get(0)?, r.get(1)?, r.get(2)?))
}
fn row4(r: &Row) -> rusqlite::Result<(String, String, i64, i64)> {
    Ok((r.get(0)?, r.get(1)?, r.get(2)?, r.get(3)?))
}
fn row5(r: &Row) -> rusqlite::Result<(String, String, i64, i64, i64)> {
    Ok((r.get(0)?, r.get(1)?, r.get(2)?, r.get(3)?, r.get(4)?))
}

/// Minimal JSON string escaping.
pub fn jq(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + 2);
    out.push('"');
    for ch in s.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

/// Escape only the body of a JSON string (no surrounding quotes).
pub fn jq_body(s: &str) -> String {
    jq(s)[1..jq(s).len() - 1].to_string()
}

/// Safe JSON parse into serde-free dynamic value tree (used by lib.rs
/// adapters where the C side sends small JSON blobs).
pub struct JsonValue(pub String);

#[allow(dead_code)]
pub fn json_mutex() -> Mutex<()> {
    Mutex::new(())
}
