use wed_core::filters::{parse_rule, Category};
fn main() {
    let text = std::fs::read_to_string("../resources/lists/easyprivacy.txt").unwrap();
    let mut accepted = 0;
    let mut rejected: Vec<&str> = Vec::new();
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('!') || line.starts_with('[') { continue; }
        if line.contains("##") || line.contains("#@#") || line.contains("#?#") { continue; }
        match parse_rule(line, Category::Tracker) {
            Some(_) => accepted += 1,
            None => { if rejected.len() < 25 { rejected.push(line); } }
        }
    }
    println!("accepted: {accepted}, rejected sample:");
    for r in rejected { println!("  REJECT: {r}"); }
}
