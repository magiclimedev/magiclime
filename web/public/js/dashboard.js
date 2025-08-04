// Cloud Dashboard JavaScript

let sensorCharts = {};

// Tab functionality
function initializeTabs() {
    const tabButtons = document.querySelectorAll('.tab-button');
    const tabContents = document.querySelectorAll('.tab-content');
    
    tabButtons.forEach(button => {
        button.addEventListener('click', () => {
            const tabName = button.getAttribute('data-tab');
            
            // Remove active class from all tabs and contents
            tabButtons.forEach(btn => btn.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));
            
            // Add active class to clicked tab and corresponding content
            button.classList.add('active');
            document.getElementById(tabName + '-tab').classList.add('active');
            
            // Initialize charts when dashboard tab is shown
            if (tabName === 'dashboard') {
                setTimeout(initializeSensorCharts, 100);
            }
        });
    });
    
    // Initialize charts on page load
    setTimeout(initializeSensorCharts, 500);
}

// WebSocket functionality
function initializeWebSocket() {
    try {
        socket = io();
        
        socket.on('connect', () => {
            console.log('Connected to WebSocket');
            updateConnectionStatus('connected');
        });
        
        socket.on('disconnect', () => {
            console.log('Disconnected from WebSocket');
            updateConnectionStatus('disconnected');
        });
        
        // Listen for sensor data updates
        socket.on('LOG_DATA', (data) => {
            handleSensorDataUpdate(data);
        });
        
        // Listen for receiver status updates
        socket.on('RECEIVER', (data) => {
            handleReceiverStatusUpdate(data.status);
        });
        
    } catch (error) {
        console.error('WebSocket initialization failed:', error);
    }
}

// Handle incoming sensor data
function handleSensorDataUpdate(data) {
    console.log('Received sensor data:', data);
    
    // Update sensor card if it exists
    const sensorCard = document.querySelector(`[data-sensor-uid="${data.uid}"]`);
    if (sensorCard) {
        updateSensorCard(sensorCard, data);
    }
    
    // Add to raw data feed
    addToRawDataFeed(data);
    
    // Update chart if exists
    if (sensorCharts[data.uid]) {
        updateSensorChart(data.uid, data);
    }
}

// Update sensor card with new data
function updateSensorCard(card, data) {
    const sensorId = card.getAttribute('data-sensor-id');
    
    // Update temperature and humidity values
    if (typeof data.data === 'object' && data.data !== null) {
        let temperature = '';
        let humidity = '';
        
        if (data.data.temperature && data.data.humidity) {
            temperature = data.data.temperature;
            humidity = data.data.humidity;
        } else if (data.data.value1 && data.data.value2) {
            temperature = data.data.value1;
            humidity = data.data.value2;
        }
        
        const tempElement = document.getElementById(`temp-${sensorId}`);
        const humidityElement = document.getElementById(`humidity-${sensorId}`);
        
        if (tempElement && temperature) {
            tempElement.textContent = temperature;
        }
        if (humidityElement && humidity) {
            humidityElement.textContent = humidity;
        }
    }
    
    // Update status indicator
    const statusElement = card.querySelector('.sensor-status');
    if (statusElement) {
        statusElement.classList.remove('dead');
        statusElement.classList.add('active');
    }
    
    // Remove dead class from card
    card.classList.remove('dead');
    card.classList.add('active');
}

// Add data to raw feed
function addToRawDataFeed(data) {
    const feed = document.getElementById('raw-data-feed');
    if (feed) {
        const timestamp = new Date().toISOString();
        const entry = document.createElement('div');
        entry.style.marginBottom = '10px';
        entry.style.paddingBottom = '10px';
        entry.style.borderBottom = '1px solid #2d3748';
        
        entry.innerHTML = `
            <div style="color: #68d391; font-size: 0.8rem;">${timestamp}</div>
            <div style="margin-top: 4px;">
                <span style="color: #90cdf4;">UID:</span> ${data.uid} |
                <span style="color: #90cdf4;">Type:</span> ${data.sensor || 'unknown'} |
                <span style="color: #90cdf4;">RSS:</span> ${data.rss} |
                <span style="color: #90cdf4;">Battery:</span> ${data.bat}%
            </div>
            <div style="margin-top: 4px; color: #fbb6ce;">
                <span style="color: #90cdf4;">Data:</span> ${JSON.stringify(data.data, null, 2)}
            </div>
        `;
        
        // Add to top of feed
        feed.insertBefore(entry, feed.firstChild);
        
        // Keep only last 50 entries
        while (feed.children.length > 50) {
            feed.removeChild(feed.lastChild);
        }
    }
}

// Initialize sensor charts
function initializeSensorCharts() {
    const sensorCards = document.querySelectorAll('.sensor-card');
    
    sensorCards.forEach(card => {
        const canvas = card.querySelector('.sensor-chart');
        if (canvas) {
            const sensorId = card.getAttribute('data-sensor-id');
            const sensorUid = card.getAttribute('data-sensor-uid');
            
            // Destroy existing chart if it exists
            if (sensorCharts[sensorUid]) {
                sensorCharts[sensorUid].destroy();
            }
            
            // Create new chart
            const ctx = canvas.getContext('2d');
            sensorCharts[sensorUid] = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Value',
                        data: [],
                        borderColor: '#667eea',
                        backgroundColor: 'rgba(102, 126, 234, 0.1)',
                        borderWidth: 2,
                        fill: true,
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: {
                            display: false
                        },
                        y: {
                            display: false
                        }
                    },
                    plugins: {
                        legend: {
                            display: false
                        }
                    },
                    elements: {
                        point: {
                            radius: 0,
                            hoverRadius: 4
                        }
                    }
                }
            });
            
            // Load initial data
            loadSensorChartData(sensorId, sensorUid);
        }
    });
}

// Load chart data for a sensor
function loadSensorChartData(sensorId, sensorUid) {
    fetch(`/api/sensor/${sensorId}/chart-data`)
        .then(response => response.json())
        .then(data => {
            if (sensorCharts[sensorUid] && data.length > 0) {
                const chart = sensorCharts[sensorUid];
                chart.data.labels = data.map(point => new Date(point.time).toLocaleTimeString());
                chart.data.datasets[0].data = data.map(point => point.value);
                chart.update('none');
            }
        })
        .catch(error => {
            console.error('Error loading chart data:', error);
        });
}

// Update sensor chart with new data
function updateSensorChart(sensorUid, data) {
    const chart = sensorCharts[sensorUid];
    if (chart) {
        const value = extractNumericValue(data.data);
        if (value !== null) {
            const now = new Date().toLocaleTimeString();
            
            // Add new data point
            chart.data.labels.push(now);
            chart.data.datasets[0].data.push(value);
            
            // Keep only last 20 points
            if (chart.data.labels.length > 20) {
                chart.data.labels.shift();
                chart.data.datasets[0].data.shift();
            }
            
            chart.update('none');
        }
    }
}

// Extract numeric value from data
function extractNumericValue(data) {
    if (typeof data === 'number') return data;
    if (typeof data === 'string') {
        const num = parseFloat(data);
        return isNaN(num) ? null : num;
    }
    if (typeof data === 'object' && data !== null) {
        if (data.value !== undefined) return parseFloat(data.value) || null;
        if (data.temperature !== undefined) return parseFloat(data.temperature) || null;
        if (data.temp !== undefined) return parseFloat(data.temp) || null;
        if (data.humidity !== undefined) return parseFloat(data.humidity) || null;
        if (data.moisture !== undefined) return parseFloat(data.moisture) || null;
        if (data.light !== undefined) return parseFloat(data.light) || null;
    }
    return null;
}

// Update connection status
function updateConnectionStatus(status) {
    // This could be used to show WebSocket connection status
    console.log('Connection status:', status);
}

// Handle receiver status updates with reconnecting logic
let lastReceiverStatus = null;
let reconnectTimeout = null;

function handleReceiverStatusUpdate(status) {
    console.log('Receiver status update:', status);
    
    // If we were disconnected and now connecting, show reconnecting state briefly
    if (lastReceiverStatus === 'disconnected' && status === 'connected') {
        updateGatewayStatus('reconnecting');
        
        // Clear any existing timeout
        if (reconnectTimeout) {
            clearTimeout(reconnectTimeout);
        }
        
        // Show connected after a brief delay
        reconnectTimeout = setTimeout(() => {
            updateGatewayStatus('connected');
        }, 1500); // Show orange for 1.5 seconds
    } else {
        // Direct status update
        updateGatewayStatus(status);
    }
    
    lastReceiverStatus = status;
}

// Update gateway status
function updateGatewayStatus(status) {
    const statusElement = document.getElementById('gateway-status');
    if (statusElement) {
        const isConnected = status === 'connected' || status === 'CON';
        const isReconnecting = status === 'reconnecting';
        
        if (isConnected) {
            statusElement.textContent = 'Connected';
            statusElement.className = 'status-indicator connected';
        } else if (isReconnecting) {
            statusElement.textContent = 'Reconnecting';
            statusElement.className = 'status-indicator reconnecting';
        } else {
            statusElement.textContent = 'Disconnected';
            statusElement.className = 'status-indicator disconnected';
        }
    }
    
    const lastSeenElement = document.getElementById('last-seen');
    if (lastSeenElement && (status === 'connected' || status === 'CON')) {
        lastSeenElement.textContent = new Date().toLocaleString();
    }
}

// Update timestamps
function updateTimestamps() {
    // Update relative timestamps
    const timestampElements = document.querySelectorAll('[id^="last-update-"]');
    timestampElements.forEach(element => {
        const timestamp = element.getAttribute('data-timestamp');
        if (timestamp) {
            element.textContent = getRelativeTime(new Date(timestamp));
        }
    });
}

// Get relative time string
function getRelativeTime(date) {
    const now = new Date();
    const diffMs = now - date;
    const diffMins = Math.floor(diffMs / 60000);
    
    if (diffMins < 1) return 'Just now';
    if (diffMins < 60) return `${diffMins}m ago`;
    
    const diffHours = Math.floor(diffMins / 60);
    if (diffHours < 24) return `${diffHours}h ago`;
    
    const diffDays = Math.floor(diffHours / 24);
    return `${diffDays}d ago`;
}

// Sensor management functions
function viewSensorDetails(sensorId) {
    window.location.href = `/${gatewaySerialNum}/sensor/${sensorId}`;
}

function updateGatewayName() {
    const nameInput = document.getElementById('gateway-name');
    const newName = nameInput.value.trim();
    
    if (newName) {
        fetch(`/api/gateway/${gatewaySerialNum}`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ name: newName })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                alert('Gateway name updated successfully');
            } else {
                alert('Failed to update gateway name');
            }
        })
        .catch(error => {
            console.error('Error updating gateway name:', error);
            alert('Error updating gateway name');
        });
    }
}

function updateSensorName(sensorId) {
    const input = document.querySelector(`input[data-sensor-id="${sensorId}"]`);
    const newName = input.value.trim();
    
    if (newName) {
        fetch(`/api/sensor/${sensorId}`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ name: newName })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                // Update the sensor card name
                const nameElement = document.querySelector(`[data-editable="${sensorId}"]`);
                if (nameElement) {
                    nameElement.textContent = newName;
                }
                alert('Sensor name updated successfully');
            } else {
                alert('Failed to update sensor name');
            }
        })
        .catch(error => {
            console.error('Error updating sensor name:', error);
            alert('Error updating sensor name');
        });
    }
}