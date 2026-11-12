# Common issues and how-to guides

## Viewer upload reports HTTP 404

A stale frontend build can call an API route that no longer exists on the server.
Run `npm --prefix viewer run build` from the repository root, then reload the page.
Rebuilding the C++ target alone does not update the frontend assets. If server
source also changed, rebuild and restart the viewer so both sides are current.

See the [viewer build and run instructions](../viewer/README.md#build-and-run).
