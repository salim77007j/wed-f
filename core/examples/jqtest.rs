fn main() {
    let mut e = wed_core::filters::Engine::new();
    e.add_list("\"&rb=&uuid=$third-party\n", wed_core::filters::Category::Ad);
    let j = wed_core::contentblocker::compile_json(&e);
    println!("{}", j);
}
