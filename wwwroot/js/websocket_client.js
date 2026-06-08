
class WebSocketClient {
    constructor(host, handlers = {}) {
        this.path = "";
        this.host = host;
        this.port = 8765;
        this.maxReconnectAttempts = 1000;
        this.reconnectAttempts = 0;
        this.socket = null;
        this._reconnectTimer = null;
        this._pending = [];

        this.onReceivedMsgHandler = handlers.onReceivedMsg || null;
        this.onErrorHandler = handlers.onError || null;
        this.onOpenHandler = handlers.onOpen || null;
        this.onCloseHandler = handlers.onClose || null;

        this.connect();
    }

    connect() {
        const protocol = window.location.protocol === "https:" ? "wss://" : "ws://";
        var wsUrl = `${protocol}${this.host}:${this.port}${this.path}`;

        console.log("Connecting to", wsUrl);

        // If there's an existing socket, close it first and wait for confirmation
        // before creating a new connection (server accepts only one client at a time).
        if (this.socket) {
            var old = this.socket;
            old.onopen = null;
            old.onclose = null;
            old.onerror = null;
            old.onmessage = null;
            if (old.readyState === WebSocket.OPEN ||
                old.readyState === WebSocket.CONNECTING) {
                old.addEventListener("close", () => this._createSocket(wsUrl));
                old.close();
                return;
            }
        }

        this._createSocket(wsUrl);
    }

    _createSocket(wsUrl) {
        var ws = new WebSocket(wsUrl);
        this.socket = ws;

        ws.addEventListener("open", () => {
            console.log("Connected to WebSocket server");
            if (typeof this.onOpenHandler === 'function') {
                this.onOpenHandler();
            }
            this.reconnectAttempts = 0;
            var pending = this._pending;
            this._pending = [];
            pending.forEach(msg => ws.send(JSON.stringify(msg)));
        });

        ws.addEventListener("message", (event) => {
            if (typeof this.onReceivedMsgHandler === 'function') {
                this.onReceivedMsgHandler(event.data);
            }
        });

        ws.addEventListener("error", () => {
            console.error("WebSocket error");
            if (typeof this.onErrorHandler === 'function') {
                this.onErrorHandler();
            }
        });

        ws.addEventListener("close", (event) => {
            console.log(`WebSocket closed (code: ${event.code}, reason: ${event.reason})`);
            if (typeof this.onCloseHandler === 'function') {
                this.onCloseHandler();
            }
            // Only reconnect if this socket hasn't been replaced (server accepts one client at a time)
            if (this.socket === ws) {
                this.reconnect();
            }
        });
    }

    send(data) {
        if (this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify(data));
        } else if (this.socket.readyState === WebSocket.CONNECTING) {
            this._pending.push(data);
        } else {
            console.warn("WebSocket is not open. Message not sent.");
        }
    }

    reconnect() {
        if (this._reconnectTimer) return;

        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error("Max reconnect attempts reached. Giving up.");
            return;
        }

        var delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts), 30000);
        console.log(`Reconnecting in ${delay / 1000} seconds...`);
        this._reconnectTimer = setTimeout(() => {
            this._reconnectTimer = null;
            this.connect();
        }, delay);
        this.reconnectAttempts++;
    }
}
