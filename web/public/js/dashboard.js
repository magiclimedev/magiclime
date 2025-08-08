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
            
            // Handle tab-specific initialization
            if (tabName === 'dashboard') {
                // Don't reinitialize charts - they're already initialized on page load
                // Cleanup raw logs WebSocket if switching away from raw data
                if (typeof cleanupRawLogsWebSocket === 'function') {
                    cleanupRawLogsWebSocket();
                }
            } else if (tabName === 'raw-data') {
                // Initialize raw logs WebSocket when switching to raw data tab
                setTimeout(async () => {
                    if (typeof initializeRawLogsWebSocket === 'function') {
                        await initializeRawLogsWebSocket();
                    }
                }, 100);
            } else {
                // Cleanup raw logs WebSocket if switching to settings or other tabs
                if (typeof cleanupRawLogsWebSocket === 'function') {
                    cleanupRawLogsWebSocket();
                }
            }
        });
    });
    
    // Charts are only used in sensor detail view, not dashboard
    // initializeSensorCharts();
}

// View toggle functionality
function initializeViewToggle() {
    const viewButtons = document.querySelectorAll('.view-btn');
    const viewContents = document.querySelectorAll('.view-content');
    
    if (!viewButtons.length) return; // Exit if no view buttons found
    
    viewButtons.forEach(button => {
        if (!button.classList.contains('disabled')) {
            button.addEventListener('click', () => {
                const viewName = button.getAttribute('data-view');
                
                // Update active button
                viewButtons.forEach(btn => btn.classList.remove('active'));
                button.classList.add('active');
                
                // Update active content
                viewContents.forEach(content => content.classList.remove('active'));
                const viewContent = document.getElementById(`${viewName}-view`);
                if (viewContent) {
                    viewContent.classList.add('active');
                }
                
                // Save preference
                localStorage.setItem('preferredView', viewName);
            });
        }
    });
    
    // Load saved preference
    const savedView = localStorage.getItem('preferredView');
    if (savedView && savedView !== 'grid') {
        const savedButton = document.querySelector(`.view-btn[data-view="${savedView}"]`);
        if (savedButton && !savedButton.classList.contains('disabled')) {
            savedButton.click();
        }
    }
}

// Auto-refresh functionality
function initializeAutoRefresh() {
    // Start auto-refresh every 3 minutes (180000ms)
    refreshInterval = setInterval(refreshSensorData, 180000);
    
    // Add manual refresh button
    addRefreshButton();
    
    // Update timestamps every 30 seconds
    setInterval(updateTimestamps, 30000);
    
    console.log('Auto-refresh initialized - updating every 3 minutes');
}

// Refresh sensor data from server
async function refreshSensorData() {
    if (isRefreshing) return; // Prevent multiple concurrent refreshes
    
    try {
        isRefreshing = true;
        updateRefreshButton(true); // Show loading state
        
        // Get current gateway serial from URL path
        const pathParts = window.location.pathname.split('/');
        const gatewaySerial = pathParts[1]; // Assumes URL format: /SERIAL/
        
        // Fetch latest sensor data
        const response = await fetch(`/api/gateway/${gatewaySerial}/sensors`);
        if (!response.ok) throw new Error('Failed to fetch sensor data');
        
        const sensors = await response.json();
        
        // Update each sensor card
        sensors.forEach(sensor => {
            updateSensorCardFromData(sensor);
        });
        
        // Update last refresh time
        updateLastRefreshTime();
        
        console.log(`Refreshed ${sensors.length} sensors`);
        
    } catch (error) {
        console.error('Failed to refresh sensor data:', error);
        showRefreshError();
    } finally {
        isRefreshing = false;
        updateRefreshButton(false); // Hide loading state
    }
}

// Update sensor card with new data
function updateSensorCard(card, data) {
    const sensorId = card.getAttribute('data-sensor-id');
    
    // This function is kept for backward compatibility but not used in auto-refresh
    console.warn('updateSensorCard called - this function is deprecated, use updateSensorCardFromData instead');
}

// Add data to raw feed - will be reimplemented for raw logs WebSocket
function addToRawDataFeed(data) {
    // This function will be handled by the raw logs WebSocket implementation
    return;
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
                
                // Show subtle confirmation by briefly changing input border to green
                input.style.borderColor = '#10b981';
                input.style.boxShadow = '0 0 0 1px #10b981';
                setTimeout(() => {
                    input.style.borderColor = '';
                    input.style.boxShadow = '';
                }, 2000);
            } else {
                // Show error with red border
                input.style.borderColor = '#ef4444';
                input.style.boxShadow = '0 0 0 1px #ef4444';
                setTimeout(() => {
                    input.style.borderColor = '';
                    input.style.boxShadow = '';
                }, 3000);
            }
        })
        .catch(error => {
            console.error('Error updating sensor name:', error);
            // Show error with red border
            input.style.borderColor = '#ef4444';
            input.style.boxShadow = '0 0 0 1px #ef4444';
            setTimeout(() => {
                input.style.borderColor = '';
                input.style.boxShadow = '';
            }, 3000);
        });
    }
}

// Auto-refresh helper functions
function addRefreshButton() {
    const header = document.querySelector('.dashboard-header') || document.querySelector('h1');
    if (!header) return;
    
    const refreshButton = document.createElement('button');
    refreshButton.id = 'manual-refresh-btn';
    refreshButton.className = 'refresh-btn';
    refreshButton.innerHTML = '🔄 Refresh';
    refreshButton.onclick = () => refreshSensorData();
    
    const lastRefreshDiv = document.createElement('div');
    lastRefreshDiv.id = 'last-refresh-time';
    lastRefreshDiv.className = 'last-refresh-time';
    lastRefreshDiv.textContent = 'Last updated: Never';
    
    const refreshContainer = document.createElement('div');
    refreshContainer.className = 'refresh-container';
    refreshContainer.style.cssText = 'display: flex; gap: 10px; align-items: center; margin-top: 10px;';
    
    refreshContainer.appendChild(refreshButton);
    refreshContainer.appendChild(lastRefreshDiv);
    
    header.parentNode.insertBefore(refreshContainer, header.nextSibling);
}

function updateRefreshButton(loading) {
    const button = document.getElementById('manual-refresh-btn');
    if (!button) return;
    
    if (loading) {
        button.innerHTML = '⏳ Refreshing...';
        button.disabled = true;
    } else {
        button.innerHTML = '🔄 Refresh';
        button.disabled = false;
    }
}

function updateLastRefreshTime() {
    const element = document.getElementById('last-refresh-time');
    if (element) {
        element.textContent = `Last updated: ${new Date().toLocaleTimeString()}`;
    }
}

function showRefreshError() {
    const element = document.getElementById('last-refresh-time');
    if (element) {
        element.textContent = 'Error refreshing data';
        element.style.color = 'red';
        setTimeout(() => {
            element.style.color = '';
        }, 3000);
    }
}

function updateSensorCardFromData(sensor) {
    const card = document.querySelector(`[data-sensor-uid="${sensor.uid}"]`);
    if (!card) return;
    
    const sensorId = sensor.id;
    
    // Update temperature and humidity values if logs exist
    if (sensor.logs && sensor.logs.length > 0) {
        const latestLog = sensor.logs[0];
        
        if (latestLog.data && typeof latestLog.data === 'object') {
            const tempElement = document.getElementById(`temp-${sensorId}`);
            const humidityElement = document.getElementById(`humidity-${sensorId}`);
            
            if (tempElement && latestLog.data.temperature) {
                tempElement.textContent = latestLog.data.temperature;
            }
            
            if (humidityElement && latestLog.data.humidity) {
                humidityElement.textContent = latestLog.data.humidity;
            }
        }
        
        // Update battery level
        const batteryElement = card.querySelector('.battery');
        if (batteryElement && latestLog.bat) {
            batteryElement.textContent = latestLog.bat + 'V';
        }
        
        // Update signal bars based on RSS
        updateSignalBars(card, latestLog.rss || 0);
        
        // Update path indicator
        const signalBarsElement = card.querySelector('.signal-bars');
        if (signalBarsElement) {
            let pathIndicatorElement = signalBarsElement.querySelector('.path-indicator');
            const isRepeater = latestLog.pathIndicator === 'p' || latestLog.path === 'repeater';
            
            if (isRepeater && !pathIndicatorElement) {
                pathIndicatorElement = document.createElement('span');
                pathIndicatorElement.className = 'path-indicator';
                pathIndicatorElement.textContent = 'R';
                signalBarsElement.appendChild(pathIndicatorElement);
            } else if (!isRepeater && pathIndicatorElement) {
                pathIndicatorElement.remove();
            }
        }
        
        // Update last updated timestamp
        const timestampElement = card.querySelector('.last-updated');
        if (timestampElement && latestLog.created_at) {
            const lastUpdate = new Date(latestLog.created_at);
            timestampElement.textContent = lastUpdate.toLocaleString();
            timestampElement.setAttribute('data-timestamp', latestLog.created_at);
        }
    }
    
    // Update sensor status
    const statusElement = card.querySelector('.sensor-status');
    if (statusElement) {
        statusElement.className = `sensor-status ${sensor.status}`;
        card.className = card.className.replace(/\b(active|dead)\b/g, '') + ` ${sensor.status}`;
    }
}

function updateSignalBars(card, rss) {
    const signalBarsElement = card.querySelector('.signal-bars');
    if (!signalBarsElement) return;
    
    const bars = signalBarsElement.querySelectorAll('.bar');
    let activeBars = 0;
    
    // Fixed thresholds: 90+ = 4 bars, matching EJS templates
    if (rss >= 90) activeBars = 4;
    else if (rss >= 70) activeBars = 3;
    else if (rss >= 50) activeBars = 2;
    else if (rss >= 30) activeBars = 1;
    
    bars.forEach((bar, index) => {
        if (index < activeBars) {
            bar.classList.add('active');
        } else {
            bar.classList.remove('active');
        }
    });
}

function updateTimestamps() {
    // Update relative timestamps for "X minutes ago" display
    const timestampElements = document.querySelectorAll('.last-updated[data-timestamp]');
    timestampElements.forEach(element => {
        const timestamp = element.getAttribute('data-timestamp');
        if (timestamp) {
            const date = new Date(timestamp);
            const now = new Date();
            const diffMinutes = Math.floor((now - date) / (1000 * 60));
            
            if (diffMinutes < 1) {
                element.textContent = 'Just now';
            } else if (diffMinutes < 60) {
                element.textContent = `${diffMinutes}m ago`;
            } else {
                const diffHours = Math.floor(diffMinutes / 60);
                element.textContent = `${diffHours}h ago`;
            }
        }
    });
}

// Table sorting functionality
function initializeTableSorting() {
    const sortableHeaders = document.querySelectorAll('.sortable');
    
    sortableHeaders.forEach(header => {
        header.addEventListener('click', () => {
            const sortField = header.getAttribute('data-sort');
            const currentSort = header.classList.contains('sort-asc') ? 'asc' : 
                               header.classList.contains('sort-desc') ? 'desc' : 'none';
            
            // Clear all other sort indicators
            sortableHeaders.forEach(h => {
                h.classList.remove('sort-asc', 'sort-desc');
            });
            
            // Determine new sort direction
            let newSort = 'asc';
            if (currentSort === 'asc') {
                newSort = 'desc';
            } else if (currentSort === 'desc') {
                newSort = 'asc';
            }
            
            // Apply sort class
            header.classList.add(`sort-${newSort}`);
            
            // Sort the table
            sortTable(sortField, newSort);
        });
    });
}

function sortTable(field, direction) {
    const tbody = document.getElementById('sensors-table-body');
    if (!tbody) return;
    
    const rows = Array.from(tbody.querySelectorAll('tr'));
    
    rows.sort((a, b) => {
        let aValue, bValue;
        
        switch (field) {
            case 'name':
                aValue = a.querySelector('.name-cell .sensor-name').textContent.toLowerCase();
                bValue = b.querySelector('.name-cell .sensor-name').textContent.toLowerCase();
                break;
                
            case 'temperature':
                aValue = parseFloat(a.querySelector('.temp-cell').textContent) || -999;
                bValue = parseFloat(b.querySelector('.temp-cell').textContent) || -999;
                break;
                
            case 'humidity':
                aValue = parseFloat(a.querySelector('.humidity-cell').textContent) || -999;
                bValue = parseFloat(b.querySelector('.humidity-cell').textContent) || -999;
                break;
                
            case 'signal':
                // Count active signal bars
                aValue = a.querySelectorAll('.signal-bars-small .bar.active').length;
                bValue = b.querySelectorAll('.signal-bars-small .bar.active').length;
                break;
                
            case 'lastUpdate':
                const aTimestamp = a.querySelector('.update-cell').getAttribute('data-timestamp') || '1970-01-01';
                const bTimestamp = b.querySelector('.update-cell').getAttribute('data-timestamp') || '1970-01-01';
                aValue = new Date(aTimestamp).getTime();
                bValue = new Date(bTimestamp).getTime();
                break;
                
            default:
                return 0;
        }
        
        // Handle string vs number comparison
        if (typeof aValue === 'string' && typeof bValue === 'string') {
            return direction === 'asc' ? aValue.localeCompare(bValue) : bValue.localeCompare(aValue);
        } else {
            return direction === 'asc' ? aValue - bValue : bValue - aValue;
        }
    });
    
    // Re-append sorted rows
    rows.forEach(row => tbody.appendChild(row));
}