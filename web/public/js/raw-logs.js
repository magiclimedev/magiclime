// Raw Logs WebSocket Client
// Only connects when user is actively viewing raw logs

class RawLogsWebSocket {
    constructor() {
        this.socket = null;
        this.isConnected = false;
        this.maxLogEntries = 100;
        this.feed = null;
    }

    async initialize() {
        this.feed = document.getElementById('raw-data-feed');
        if (!this.feed) {
            console.error('Raw data feed element not found');
            return;
        }

        // Add connection status indicator
        this.addConnectionStatus();
        
        // Load initial historical data
        await this.loadInitialData();
        
        // Connect WebSocket for real-time updates
        this.connect();
        
        // Add cleanup on page navigation
        this.setupCleanup();
    }

    async loadInitialData() {
        try {
            // Get gateway serial from the page URL or data attribute
            const pathParts = window.location.pathname.split('/');
            const gatewaySerial = pathParts[pathParts.length - 1];
            
            if (!gatewaySerial) {
                console.error('No gateway serial found in URL');
                return;
            }
            
            console.log('Loading initial raw logs data...');
            const response = await fetch(`/api/gateway/${gatewaySerial}/raw-logs?limit=20`);
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const rawLogs = await response.json();
            
            // Display logs in reverse chronological order (newest first)
            rawLogs.forEach(log => {
                this.addHistoricalLogEntry(log);
            });
            
            console.log(`Loaded ${rawLogs.length} historical raw log entries`);
            
        } catch (error) {
            console.error('Failed to load initial raw logs:', error);
            this.addSystemEntry('System', 'Failed to load historical data');
        }
    }

    connect() {
        try {
            console.log('Connecting to raw logs WebSocket...');
            this.socket = io();
            
            this.socket.on('connect', () => {
                this.isConnected = true;
                this.updateConnectionStatus('connected');
                console.log('Raw logs WebSocket connected');
            });
            
            this.socket.on('disconnect', () => {
                this.isConnected = false;
                this.updateConnectionStatus('disconnected');
                console.log('Raw logs WebSocket disconnected');
            });
            
            // Listen for real-time log data
            this.socket.on('LOG_DATA', (data) => {
                this.addLogEntry(data);
            });
            
            // Listen for receiver status
            this.socket.on('RECEIVER', (data) => {
                this.addSystemEntry('Receiver Status', data.status);
            });
            
        } catch (error) {
            console.error('Failed to connect raw logs WebSocket:', error);
            this.updateConnectionStatus('error');
        }
    }

    disconnect() {
        if (this.socket) {
            console.log('Disconnecting raw logs WebSocket...');
            this.socket.disconnect();
            this.socket = null;
            this.isConnected = false;
            this.updateConnectionStatus('disconnected');
        }
    }

    cleanup() {
        // Disconnect WebSocket
        this.disconnect();
        
        // Remove connection status indicator
        const existingStatus = document.getElementById('raw-logs-connection');
        if (existingStatus) {
            existingStatus.remove();
        }
        
        // Clear the feed
        if (this.feed) {
            this.feed.innerHTML = '';
        }
    }

    addConnectionStatus() {
        // Remove existing status indicator if it exists
        const existingStatus = document.getElementById('raw-logs-connection');
        if (existingStatus) {
            existingStatus.remove();
        }
        
        const statusDiv = document.createElement('div');
        statusDiv.id = 'raw-logs-connection';
        statusDiv.className = 'connection-status';
        statusDiv.innerHTML = `
            <span class="status-label">Real-time feed:</span>
            <span class="status-indicator" id="raw-logs-status">Connecting...</span>
        `;
        
        // Insert before the feed
        this.feed.parentNode.insertBefore(statusDiv, this.feed);
    }

    updateConnectionStatus(status) {
        const statusElement = document.getElementById('raw-logs-status');
        if (!statusElement) return;
        
        statusElement.className = `status-indicator ${status}`;
        
        switch (status) {
            case 'connected':
                statusElement.textContent = 'Connected';
                break;
            case 'disconnected':
                statusElement.textContent = 'Disconnected';
                break;
            case 'error':
                statusElement.textContent = 'Connection Error';
                break;
            default:
                statusElement.textContent = 'Connecting...';
        }
    }

    addLogEntry(data) {
        if (!this.feed) return;
        
        const entry = document.createElement('div');
        entry.className = 'log-entry';
        
        // Format timestamp in CST with milliseconds
        const now = new Date();
        const cstTime = this.formatCST(now, true); // true for milliseconds
        
        // Determine packet type tags
        const isDuplicate = data.is_duplicate || false;
        const pathType = data.path === 'repeater' ? 'REPEATER' : 'DIRECT';
        const duplicateType = isDuplicate ? 'DUPLICATE' : 'FIRST';
        
        // Get timing information for duplicates
        let timingInfo = '';
        if (isDuplicate && data.duplicate_info && data.duplicate_info.timeDiff) {
            timingInfo = ` (+${data.duplicate_info.timeDiff}ms after ${data.duplicate_info.originalPath.toUpperCase()})`;
        }
        
        // Use the actual raw packet data from gateway
        // Remove the extra metadata added by the web server
        const rawPacket = { ...data };
        delete rawPacket.timestamp;
        delete rawPacket.gateway_serial_num;
        delete rawPacket.log_id;
        delete rawPacket.is_duplicate;
        delete rawPacket.duplicate_info;
        
        entry.innerHTML = `
            <div class="tagged-log-entry">
                <div class="log-header">
                    <span class="log-timestamp">${cstTime}</span>
                    <span class="path-tag ${pathType.toLowerCase()}">${pathType}</span>
                    <span class="duplicate-tag ${duplicateType.toLowerCase()}">${duplicateType}</span>
                    ${timingInfo ? `<span class="timing-info">${timingInfo}</span>` : ''}
                </div>
                <pre class="raw-json">${JSON.stringify(rawPacket, null, 2)}</pre>
            </div>
        `;
        
        // Add to top of feed
        this.feed.insertBefore(entry, this.feed.firstChild);
        
        // Limit number of entries
        this.limitLogEntries();
    }

    addHistoricalLogEntry(log) {
        if (!this.feed) return;
        
        const entry = document.createElement('div');
        entry.className = 'log-entry';
        
        // Parse the timestamp from the database and format in CST with milliseconds
        const timestamp = new Date(log.created_at);
        const cstTime = this.formatCST(timestamp, true);
        
        // Determine path type from historical data
        const pathType = (log.path === 'repeater' || log.path_indicator === 'R') ? 'REPEATER' : 'DIRECT';
        
        // The log.data field should now contain the complete raw packet JSON
        let rawPacketData;
        try {
            rawPacketData = typeof log.data === 'string' ? JSON.parse(log.data) : log.data;
        } catch (e) {
            // Fallback for old format
            rawPacketData = {
                uid: log.uid,
                sensor_type: log.sensor_type,
                rss: log.rss,
                bat: log.bat,
                data: log.data,
                path: log.path,
                path_indicator: log.path_indicator
            };
        }
        
        entry.innerHTML = `
            <div class="tagged-log-entry">
                <div class="log-header">
                    <span class="log-timestamp">${cstTime}</span>
                    <span class="path-tag ${pathType.toLowerCase()}">${pathType}</span>
                    <span class="duplicate-tag historical">HISTORICAL</span>
                </div>
                <pre class="raw-json">${JSON.stringify(rawPacketData, null, 2)}</pre>
            </div>
        `;
        
        // Add to bottom of feed (historical data should be in reverse chronological order)
        this.feed.appendChild(entry);
    }

    formatCST(date, includeMilliseconds = false) {
        // Convert to Central Time (handles both CST and CDT)
        // CDT is UTC-5 (March to November), CST is UTC-6 (November to March)
        const isDST = this.isDaylightSavingTime(date);
        const centralOffset = isDST ? -5 * 60 : -6 * 60; // CDT is UTC-5, CST is UTC-6
        const utc = date.getTime() + (date.getTimezoneOffset() * 60000);
        const centralTime = new Date(utc + (centralOffset * 60000));
        
        const options = {
            month: '2-digit',
            day: '2-digit', 
            year: 'numeric',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: true
        };
        
        let formatted = centralTime.toLocaleString('en-US', options);
        
        if (includeMilliseconds) {
            const ms = centralTime.getMilliseconds().toString().padStart(3, '0');
            // Insert milliseconds before AM/PM
            formatted = formatted.replace(/(\d{2}:\d{2}:\d{2})\s*(AM|PM)/, `$1.${ms} $2`);
        }
        
        return formatted;
    }

    isDaylightSavingTime(date) {
        // Determine if the date is in daylight saving time for US Central Time
        const year = date.getFullYear();
        
        // DST starts on the second Sunday in March
        const dstStart = new Date(year, 2, 1); // March 1
        dstStart.setDate(1 + (7 - dstStart.getDay()) % 7 + 7); // Second Sunday
        dstStart.setHours(2, 0, 0, 0);
        
        // DST ends on the first Sunday in November
        const dstEnd = new Date(year, 10, 1); // November 1
        dstEnd.setDate(1 + (7 - dstEnd.getDay()) % 7); // First Sunday
        dstEnd.setHours(2, 0, 0, 0);
        
        return date >= dstStart && date < dstEnd;
    }

    addSystemEntry(type, message) {
        if (!this.feed) return;
        
        const timestamp = new Date().toISOString();
        const entry = document.createElement('div');
        entry.className = 'log-entry system-event';
        
        entry.innerHTML = `
            <div class="log-timestamp">${new Date(timestamp).toLocaleTimeString()}</div>
            <div class="log-content">
                <span class="log-field"><span class="label">${type}:</span> ${message}</span>
            </div>
        `;
        
        // Add to top of feed
        this.feed.insertBefore(entry, this.feed.firstChild);
        
        // Limit number of entries
        this.limitLogEntries();
    }

    limitLogEntries() {
        const entries = this.feed.querySelectorAll('.log-entry');
        if (entries.length > this.maxLogEntries) {
            // Remove oldest entries
            for (let i = this.maxLogEntries; i < entries.length; i++) {
                entries[i].remove();
            }
        }
    }

    setupCleanup() {
        // Disconnect when user navigates away
        window.addEventListener('beforeunload', () => {
            this.disconnect();
        });
        
        // Also disconnect when user switches tabs
        const tabButtons = document.querySelectorAll('.tab-button');
        tabButtons.forEach(button => {
            button.addEventListener('click', (e) => {
                const tabName = e.target.getAttribute('data-tab');
                if (tabName !== 'raw-data' && this.isConnected) {
                    this.disconnect();
                }
            });
        });
    }
}

// Initialize when raw data tab is shown
async function initializeRawLogsWebSocket() {
    if (window.rawLogsWS) {
        window.rawLogsWS.disconnect();
    }
    
    window.rawLogsWS = new RawLogsWebSocket();
    await window.rawLogsWS.initialize();
}

// Clean up when tab is hidden
function cleanupRawLogsWebSocket() {
    if (window.rawLogsWS) {
        window.rawLogsWS.cleanup();
        window.rawLogsWS = null;
    }
}