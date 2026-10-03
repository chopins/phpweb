## About
phpweb is a webview-based PHP SAPI.
#### The project is experimental.

## Features:

1. Display all output from PHP code in a webview. Similar to CLI, where the PHP engine executes a PHP file and outputs the content to the terminal, phpweb executes a PHP file and outputs all content to a webview, and the webview renders HTML content normally.
2. On Linux, it depends on webkitgtk-6 with a version greater than 2.40. It is currently not implemented on Windows.
3. Implement, through a custom protocol, the conversion of protocol-less links in HTML into opening local files.
