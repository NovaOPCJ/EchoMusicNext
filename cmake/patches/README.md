# curl HTTP Digest backend

`curl-8.23.0-native-http-digest.patch` applies to the unmodified [curl 8.23.0 archive](https://curl.se/download/curl-8.23.0.tar.gz), SHA-256 `e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49`. Upstream copyright and the curl license remain in the source files and [license materials](../../docs/licenses/curl-LICENSE.txt).

The private `ECHO_CURL_NATIVE_HTTP_DIGEST` definition selects curl's native HTTP Digest encoder, parser, cleanup and state layout even when SSPI is enabled. This preserves explicit usernames and the server's realm, and avoids the [SSPI SHA-256 limitation](https://curl.se/docs/knownbugs.html#SHA_256_digest_not_supported_in). Schannel and NTLM retain their SSPI implementations. SASL DIGEST-MD5 message generation also retains its original backend; the application disables the mail protocols.

Define the macro for **every translation unit in libcurl**, never for individual source files: `digestdata` is embedded in shared private structures. The application consumes only curl's public ABI and does not need the definition. Without the definition, the patch retains upstream backend selection.

`ApplyCurlPatch.cmake` checks before applying, accepts an already applied patch and rejects incompatible sources. When upgrading curl, rebase the patch against the new pinned archive and run `proxy-authentication` on Windows with Schannel / SSPI, including Basic, NTLM, Digest MD5 / SHA-256, plain / backslash / slash usernames, HTTP / TLS proxies and HTTP / CONNECT requests. A passing macOS or Linux run does not verify the Windows branch.
