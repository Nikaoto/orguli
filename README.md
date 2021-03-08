# orguli
orguli is a small tool that takes a markdown file with an optional style sheet
and outputs html.

The only feature in orguli not present in markdown is the `@filename` specifier,
which embeds images and text directly.

## Usage
For single-file documents:
```bash
orguli -s readme.md style.css > readme.html
```

For inline html (like piping into static site generators):
```bash
# Processes and converts all html files to md
for file in *.html
do
    process-file $file | orguli /dev/stdin style.css > ${file%.html}.md
done
```

I use orguli to render README files in my [private git repositories](git.nikaoto.com)
by passing orguli to cgit.

## Install
Run `./build.sh` and then `sudo install -m755 orguli /usr/local/bin/.`

## Why?
- No parser does what I want. Most of them have weird ways of behaving, which
  are not documented and only defined in code.
- Too many unnecessary features. The Markdown language is
  [surprizingly large](https://spec.commonmark.org/current/) and most people
  don't even use half of it, especially when it comes to README.md files.
- Extending the other tools is hard. A single C99 file is easier to modify and
  I've found that it's faster to just extend orguli than learn the quirks of
  some large parser though trial and error. orguli also has it's quirks as well,
  but they're easier to learn and avoid.

## Changes
orguli is a fork of rxi's [doq](https://github.com/rxi/doq), which now seems to
be abandoned. Here is a list of the changes I introduced.

### Added
- support for `<inline links like this>`
- support for `![inline images](image_link)]`
- support for nested fenced code blocks
- support for `<pre><code> code blocks </code></pre>` with edge2
- add support nested lists (only two levels deep!)
- single line lookahead (to support h1 and h2 with '====' and h2 '----')
- `-s|--single-file` option, which outputs `<head>` and others. When omitted,
  output without `<head>`
- `-h|--help` option
- support for autoclosing tags; drop inline flags at edges of headers, lists,
  and blocks
- support for `**`(strong) and `__`(em).
- fix for false em, strike, and strong detection on separate chars:<br>
  `2 * 10` will no longer turn into `2 <em> 10`, but `2 *10` will correctly turn
  into `2 <em>10`
- support for `<hr>` (--- for `<hr>`, === for `<hr class="thick">`)
- support for inline html, including `<br>` tags
- comments where necessary
- `.markdown-body` class to main `<div>` or `<body>`
- `style.css` file
- support for h4 and h5
- support for indented code blocks

  this will be turned into code
  
Just like
```
this fenced code block
```

### Removed
- dependency on `stdbool.h`
- static scope specifiers from all functions
- automatic escape on all text; If you want something escaped, use
  backslashes. For example: `\<` turns into `&lt;`

## License
This library is free software; you can redistribute is and/or modify it under
the terms of the MIT license. See [LICENSE](./LICENSE) for details.
