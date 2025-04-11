
class WebSocketClient {
    constructor(host, received_msg_handler) {
        this.path = ""
        this.host = host;
        this.port = 8765
        this.maxReconnectAttempts = 1000;
        this.reconnectAttempts = 0;
        this.socket = null;
        this.received_msg_handler = received_msg_handler;
        this.connect();
    }

    connect() {
        const protocol = window.location.protocol === "https:" ? "wss://" : "ws://";
        const wsUrl = `${protocol}${this.host}:${this.port}${this.path}`;

        console.log("Connecting to", wsUrl);
        this.socket = new WebSocket(wsUrl);

        this.socket.addEventListener("open", () => {
            console.log("Connected to WebSocket server");
            this.reconnectAttempts = 0; // Reset on successful connection
        });

        this.socket.addEventListener("message", (event) => {
            this.received_msg_handler(event.data);
        });

        this.socket.addEventListener("error", (error) => {
            console.error("WebSocket error:", error);
        });

        this.socket.addEventListener("close", (event) => {
            console.log(`WebSocket closed (code: ${event.code}, reason: ${event.reason})`);
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
