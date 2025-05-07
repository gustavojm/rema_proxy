
class WebSocketClient {
    constructor(host) {
        this.path = ""
        this.host = host;
        this.port = 8765
        this.maxReconnectAttempts = 1000;
        this.reconnectAttempts = 0;
        this.socket = null;
        this.onReceivedMsgHandler = null;
        this.onErrorHandler = null;
        this.onOpenHandler = null;
        this.onOpenHandler = null;
        this.onCloseHandler = null;
        this.connect();        
    }

    connect() {
        const protocol = window.location.protocol === "https:" ? "wss://" : "ws://";
        const wsUrl = `${protocol}${this.host}:${this.port}${this.path}`;

        console.log("Connecting to", wsUrl);
        this.socket = new WebSocket(wsUrl);

        this.socket.addEventListener("open", () => {
            console.log("Connected to WebSocket server");
            if (typeof this.onOpenHandler === 'function') {
                this.onOpenHandler();
            }
            this.reconnectAttempts = 0; // Reset on successful connection
        });

        this.socket.addEventListener("message", (event) => {
            if (typeof this.onReceivedMsgHandler === 'function') {
                this.onReceivedMsgHandler(event.data);
            }
        });

        this.socket.addEventListener("error", (error) => {
            console.error("WebSocket error:", error);
            if (typeof this.onErrorHandler === 'function') {
                this.onErrorHandler();
            }
        });

        this.socket.addEventListener("close", (event) => {
            console.log(`WebSocket closed (code: ${event.code}, reason: ${event.reason})`);
            if (typeof this.onCloseHandler === 'function') {
                this.onCloseHandler();
            }
            this.reconnect();
        });
    }

    send(data) {
        if (this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify(data));
        } else {
            console.warn("WebSocket is not open. Message not sent.");
        }
    }

    reconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            const delay = Math.min(1000 * 2 ** this.reconnectAttempts, 30000); // Exponential backoff (max 30s)
            console.log(`Reconnecting in ${delay / 1000} seconds...`);
            setTimeout(() => this.connect(), delay);
            this.reconnectAttempts++;
        } else {
            console.error("Max reconnect attempts reached. Giving up.");
        }
    }
}
