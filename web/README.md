# Web server-list widget

`server-list.html` is a self-contained, framework-free page that shows the
active Dealer's Choice servers a registry knows about. It reads the
`servers.json` that `dealers-choice-registry --json <path>` writes.

## How it fits together

1. The registry writes `servers.json` (a JSON array of verified servers).
2. A web server serves that file. **It must send a CORS header** so a page on a
   different origin can read it:

       add_header Access-Control-Allow-Origin "*";   # nginx, on the servers.json location

3. `server-list.html` fetches the JSON and renders a table, refreshing every 30
   seconds. Point it at the registry with the `src` query parameter:

       server-list.html?src=https://registry.example.org/servers.json

   With no `src`, it loads `./servers.json` next to the page (handy when the
   registry host serves the page and the JSON from the same place — then no CORS
   header is needed).

## Embedding it elsewhere

Drop an iframe into any page:

```html
<iframe src="https://your.site/server-list.html?src=https://registry.example.org/servers.json"
        style="width:100%;height:400px;border:0"></iframe>
```

To match a site's styling instead of using the iframe, copy the `<script>` from
`server-list.html` into the page and give it a `#dc-servers` container; it has
no dependencies.

## servers.json shape

A JSON array; each entry is one verified server:

```json
[
  {"ip":"203.0.113.5","port":22777,"name":"My Table","players":2,"max":5,
   "password":true,"in_progress":false}
]
```

A server name can be anything an operator sets, so the widget HTML-escapes it.
