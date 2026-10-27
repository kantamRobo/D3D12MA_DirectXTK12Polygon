use std::fs;
use std::io::{BufRead, BufReader, Read, Write};
use std::net::{TcpListener, TcpStream};
use std::process::Command;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::thread;

static REQUEST_COUNTER: AtomicUsize = AtomicUsize::new(0);

fn main() {
    let port = 8040;
    let addr = format!("127.0.0.1:{}", port);
    let listener = match TcpListener::bind(&addr) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("Failed to bind to {}: {}", addr, e);
            std::process::exit(1);
        }
    };

    println!("======================================================");
    println!("  PBRT-v4 Practice & Review App (Chapters 2 - 10)");
    println!("  Listening on http://localhost:{}", port);
    println!("  Rust Toolchain verification: Ready");
    println!("======================================================");

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                thread::spawn(|| {
                    if let Err(e) = handle_client(stream) {
                        eprintln!("Error handling client: {}", e);
                    }
                });
            }
            Err(e) => {
                eprintln!("Connection failed: {}", e);
            }
        }
    }
}

fn handle_client(mut stream: TcpStream) -> std::io::Result<()> {
    let mut reader = BufReader::new(stream.try_clone()?);
    let mut request_line = String::new();
    if reader.read_line(&mut request_line)? == 0 {
        return Ok(());
    }

    let parts: Vec<&str> = request_line.split_whitespace().collect();
    if parts.len() < 2 {
        return Ok(());
    }
    let method = parts[0];
    let path = parts[1];

    let mut content_length: usize = 0;
    loop {
        let mut line = String::new();
        if reader.read_line(&mut line)? == 0 || line == "\r\n" || line == "\n" {
            break;
        }
        let lower = line.to_lowercase();
        if lower.starts_with("content-length:") {
            if let Some(val) = lower.split(':').nth(1) {
                content_length = val.trim().parse().unwrap_or(0);
            }
        }
    }

    let mut body = vec![0u8; content_length];
    if content_length > 0 {
        reader.read_exact(&mut body)?;
    }

    if method == "OPTIONS" {
        let options_resp = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
        stream.write_all(options_resp.as_bytes())?;
        return Ok(());
    }

    if method == "GET" && (path == "/" || path == "/index.html") {
        send_file_response(&mut stream, "web/index.html", "text/html; charset=utf-8")?;
    } else if method == "GET" && path == "/api/questions" {
        send_file_response(&mut stream, "web/questions.json", "application/json; charset=utf-8")?;
    } else if method == "POST" && path == "/api/verify" {
        handle_verify(&mut stream, &body)?;
    } else {
        let not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        stream.write_all(not_found.as_bytes())?;
    }

    Ok(())
}

fn send_file_response(stream: &mut TcpStream, file_path: &str, content_type: &str) -> std::io::Result<()> {
    match fs::read(file_path) {
        Ok(bytes) => {
            let header = format!(
                "HTTP/1.1 200 OK\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                content_type,
                bytes.len()
            );
            stream.write_all(header.as_bytes())?;
            stream.write_all(&bytes)?;
        }
        Err(_) => {
            let not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            stream.write_all(not_found.as_bytes())?;
        }
    }
    Ok(())
}

fn handle_verify(stream: &mut TcpStream, body_bytes: &[u8]) -> std::io::Result<()> {
    let body_str = String::from_utf8_lossy(body_bytes);
    let (code, test_code) = parse_verify_json(&body_str);

    let (success, stdout, stderr) = compile_and_run_tests(&code, &test_code);

    let json_resp = format!(
        "{{\"success\":{},\"stdout\":{},\"stderr\":{}}}",
        success,
        escape_json_string(&stdout),
        escape_json_string(&stderr)
    );

    let header = format!(
        "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
        json_resp.len()
    );
    stream.write_all(header.as_bytes())?;
    stream.write_all(json_resp.as_bytes())?;
    Ok(())
}

fn compile_and_run_tests(code: &str, test_code: &str) -> (bool, String, String) {
    let req_id = REQUEST_COUNTER.fetch_add(1, Ordering::SeqCst);
    let temp_dir = std::env::temp_dir();
    let file_stem = format!("pbrt_toy_{}_{}", std::process::id(), req_id);
    let rs_path = temp_dir.join(format!("{}.rs", file_stem));
    let exe_path = temp_dir.join(format!("{}.exe", file_stem));
    let pdb_path = temp_dir.join(format!("{}.pdb", file_stem));

    let full_source = format!(
        "#![allow(dead_code, unused_variables, unused_imports)]\n\n{}\n\n{}",
        code,
        test_code
    );

    if let Err(e) = fs::write(&rs_path, &full_source) {
        return (false, String::new(), format!("Failed to write temp source: {}", e));
    }

    // Compile with `rustc --test`
    let compile_output = match Command::new("rustc")
        .arg("--test")
        .arg(&rs_path)
        .arg("-o")
        .arg(&exe_path)
        .output()
    {
        Ok(out) => out,
        Err(e) => {
            let _ = fs::remove_file(&rs_path);
            return (false, String::new(), format!("Failed to execute rustc: {}", e));
        }
    };

    if !compile_output.status.success() {
        let stderr = String::from_utf8_lossy(&compile_output.stderr).to_string();
        let _ = fs::remove_file(&rs_path);
        let _ = fs::remove_file(&exe_path);
        let _ = fs::remove_file(&pdb_path);
        return (false, String::new(), stderr);
    }

    // Run the compiled test binary
    let run_output = match Command::new(&exe_path).output() {
        Ok(out) => out,
        Err(e) => {
            let _ = fs::remove_file(&rs_path);
            let _ = fs::remove_file(&exe_path);
            let _ = fs::remove_file(&pdb_path);
            return (false, String::new(), format!("Failed to run test binary: {}", e));
        }
    };

    let stdout = String::from_utf8_lossy(&run_output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&run_output.stderr).to_string();
    let success = run_output.status.success();

    // Clean up
    let _ = fs::remove_file(&rs_path);
    let _ = fs::remove_file(&exe_path);
    let _ = fs::remove_file(&pdb_path);

    (success, stdout, stderr)
}

fn parse_verify_json(json: &str) -> (String, String) {
    let extract = |key: &str| -> String {
        let pattern = format!("\"{}\":", key);
        if let Some(pos) = json.find(&pattern) {
            let rest = &json[pos + pattern.len()..];
            let trimmed = rest.trim_start();
            if trimmed.starts_with('"') {
                let mut unescaped = String::new();
                let mut chars = trimmed[1..].chars();
                let mut in_escape = false;
                while let Some(c) = chars.next() {
                    if in_escape {
                        match c {
                            'n' => unescaped.push('\n'),
                            'r' => unescaped.push('\r'),
                            't' => unescaped.push('\t'),
                            '\\' => unescaped.push('\\'),
                            '"' => unescaped.push('"'),
                            _ => {
                                unescaped.push('\\');
                                unescaped.push(c);
                            }
                        }
                        in_escape = false;
                    } else if c == '\\' {
                        in_escape = true;
                    } else if c == '"' {
                        break;
                    } else {
                        unescaped.push(c);
                    }
                }
                return unescaped;
            }
        }
        String::new()
    };

    (extract("code"), extract("test_code"))
}

fn escape_json_string(s: &str) -> String {
    let mut out = String::from("\"");
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c < ' ' => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_json() {
        let json = "{\"code\":\"pub fn foo() -> i32 {\\n    42\\n}\",\"test_code\":\"#[test]\\nfn t() {\\n    assert_eq!(foo(), 42);\\n}\"}";
        let (code, test_code) = parse_verify_json(json);
        assert_eq!(code, "pub fn foo() -> i32 {\n    42\n}");
        assert_eq!(test_code, "#[test]\nfn t() {\n    assert_eq!(foo(), 42);\n}");
    }
}
