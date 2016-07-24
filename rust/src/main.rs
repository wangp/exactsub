/*
** exactsub FROM TO
**
** Replace all occurrences of FROM with TO, from standard input to standard
** output.  The FROM and TO arguments may contain any bytes (except NUL);
** there are no special characters.
*/

use std::io;
use std::io::{Read, Write};
use std::process;

extern crate libc;
use libc::size_t;

extern {
    fn memmem(haystack: *const u8, haystacklen: size_t,
              needle: *const u8, needlelen: size_t) -> *const u8;
}

fn main() {
    let argv: Vec<String> = std::env::args().collect();
    if argv.len() != 3 {
        usage()
    }
    let from = argv[1].as_bytes();
    let to = argv[2].as_bytes();
    let occurs = exactsubst(from, to, &mut io::stdin(), &mut io::stdout());
    if false {
        println!("occurrences: {}", occurs); // XXX stderr
    }
}

fn usage() -> ! {
    println!("Usage: exactsub FROM TO");
    process::exit(1);
}

fn die(msg: &'static str, err: io::Error) -> ! {
    // This sucks.
    writeln!(&mut io::stderr(), "{}: {}", msg, err)
        .expect("Could not write to stderr");
    process::exit(1);
}

fn exactsubst(from: &[u8], to: &[u8], inp: &mut Read, out: &mut Write) -> u32 {
    if from.len() == 0 {
        return exactsubst0(to, inp, out);
    }

    const MINBUFCAP : usize = 1024;
    let choosecap =
        if from.len() < MINBUFCAP {
            MINBUFCAP
        } else {
            usize::next_power_of_two(from.len()) // XXX unspecified on overflow
        };
    assert!(choosecap >= from.len(), "insufficient buffer capacity");

    let mut buf: Vec<u8> = vec![0; choosecap];
    let mut buf_len = 0;
    let mut occurs = 0;

    loop {
        let num_read = checked_read(inp, &mut buf[buf_len..]);
        if num_read == 0 { // EOF
            break
        }
        buf_len += num_read;

        if buf_len >= from.len() {
            let mut start = 0;

            loop {
                match safe_memmem(&buf[start..buf_len], from) {
                    None =>
                        break,
                    Some(k) => {
                        let found = start + k;
                        checked_write(out, &buf[start..found]);
                        checked_write(out, to);
                        start = found + from.len();
                        occurs += 1;
                    }
                }
            }

            // We can safely consume everything up to this point,
            // if we have not already done so.
            let skipto = buf_len - (from.len() - 1);
            if skipto > start {
                checked_write(out, &buf[start..skipto]);
                buf_len = shift(&mut buf, skipto, buf_len);
            } else {
                buf_len = shift(&mut buf, start, buf_len);
            }
        }
    }

    checked_write(out, &buf[0..buf_len]);

    return occurs;
}

fn exactsubst0(to: &[u8], inp: &mut Read, out: &mut Write) -> u32 {
    let mut occurs = 0;

    loop {
        let mut c: [u8; 1] = [0];
        let n = checked_read(inp, &mut c);
        checked_write(out, &to);
        occurs += 1;
        if n == 0 { // EOF
            break
        }
        checked_write(out, &c);
    }

    return occurs;
}

fn checked_read(f: &mut Read, buf: &mut [u8]) -> usize {
    match f.read(&mut buf[..]) {
        Ok(n) => return n,
        Err(e) => die("read error", e)
    }
}

fn checked_write(f: &mut Write, buf: &[u8]) {
    match f.write_all(buf) {
        Ok(_n) => (),
        Err(e) => die("write error", e)
    }
}

fn shift(buf: &mut [u8], start: usize, end: usize) -> usize {
    for i in start..end {
        buf[i-start] = buf[i];
    }
    return end - start;
}

fn safe_memmem(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    unsafe {
        let r = memmem(haystack.as_ptr(), haystack.len(),
                        needle.as_ptr(), needle.len());
        if r.is_null() {
            None
        } else {
            Some(r as usize - haystack.as_ptr() as usize)
        }
    }
}
