/**
 * Server-side Packet Deduplication Service
 * 
 * Handles deduplication of sensor packets that arrive via multiple paths.
 * Determines which packet to use for main dashboard display while storing all packets.
 */

class PacketDeduplicationService {
  constructor() {
    this.recentPackets = new Map(); // key -> {timestamp, path, logId, data}
    this.windowMs = 5000; // 5 second window for deduplication
    this.cleanupInterval = 60000; // Cleanup every minute
    
    // Start periodic cleanup
    this.startCleanup();
  }

  /**
   * Process incoming packet and determine if it should update main sensor display
   * @param {Object} packet - The sensor packet data
   * @param {number} logId - The database log ID
   * @returns {Object} - {shouldUpdate: boolean, isFirstPacket: boolean, duplicateInfo: Object}
   */
  processPacket(packet, logId) {
    const { uid, data, path, rss, bat } = packet;
    const now = Date.now();
    
    // Create deduplication key based on sensor data
    const signature = this.createPacketSignature(packet);
    const key = `${uid}:${signature}`;
    
    const existing = this.recentPackets.get(key);
    
    if (existing) {
      // This is a duplicate - don't update main display
      const timeDiff = now - existing.timestamp;
      
      // Duplicate packet filtered (debug only)
      
      // Add this packet to the duplicates list
      existing.duplicates = existing.duplicates || [];
      existing.duplicates.push({
        logId,
        path,
        timestamp: now,
        rss,
        bat
      });
      
      return {
        shouldUpdate: false,
        isFirstPacket: false,
        duplicateInfo: {
          originalPath: existing.path,
          originalTimestamp: existing.timestamp,
          timeDiff
        }
      };
    }
    
    // This is the first packet for this data - use it for main display
    this.recentPackets.set(key, {
      timestamp: now,
      path,
      logId,
      data,
      rss,
      bat,
      duplicates: []
    });
    
    // First packet received for sensor (debug only)
    
    return {
      shouldUpdate: true,
      isFirstPacket: true,
      duplicateInfo: null
    };
  }

  /**
   * Create packet signature for deduplication
   * @param {Object} packet 
   * @returns {string}
   */
  createPacketSignature(packet) {
    const { data, uid } = packet;
    
    // Create signature based on actual sensor data values
    if (data && typeof data === 'object') {
      // For structured data, use all values
      const values = [];
      if (data.value1 !== undefined) values.push(`v1:${data.value1}`);
      if (data.value2 !== undefined) values.push(`v2:${data.value2}`);
      if (data.temperature !== undefined) values.push(`temp:${data.temperature}`);
      if (data.humidity !== undefined) values.push(`hum:${data.humidity}`);
      
      return values.join('|') || 'empty';
    }
    
    // Fallback to string representation
    return String(data || 'null');
  }

  /**
   * Get statistics about deduplication
   */
  getStats() {
    const stats = {
      trackedPackets: this.recentPackets.size,
      totalDuplicates: 0,
      pathBreakdown: {
        direct: 0,
        repeater: 0
      }
    };

    for (const entry of this.recentPackets.values()) {
      if (entry.duplicates && entry.duplicates.length > 0) {
        stats.totalDuplicates += entry.duplicates.length;
      }
      
      if (entry.path === 'repeater') {
        stats.pathBreakdown.repeater++;
      } else {
        stats.pathBreakdown.direct++;
      }
    }

    return stats;
  }

  /**
   * Get duplicate information for a specific log entry
   * @param {number} logId 
   * @returns {Object|null}
   */
  getDuplicateInfo(logId) {
    for (const [key, entry] of this.recentPackets.entries()) {
      if (entry.logId === logId) {
        return {
          isOriginal: true,
          duplicates: entry.duplicates || [],
          signature: key
        };
      }
      
      if (entry.duplicates) {
        const duplicate = entry.duplicates.find(d => d.logId === logId);
        if (duplicate) {
          return {
            isOriginal: false,
            originalLogId: entry.logId,
            originalPath: entry.path,
            signature: key
          };
        }
      }
    }
    
    return null;
  }

  /**
   * Start cleanup timer
   */
  startCleanup() {
    this.cleanupTimer = setInterval(() => {
      this.cleanup();
    }, this.cleanupInterval);
  }

  /**
   * Clean up old entries
   */
  cleanup() {
    const now = Date.now();
    let cleaned = 0;
    
    for (const [key, entry] of this.recentPackets.entries()) {
      if (now - entry.timestamp > this.windowMs * 2) {
        this.recentPackets.delete(key);
        cleaned++;
      }
    }
    
    if (cleaned > 0) {
      // Cleaned up old packets (debug only)
    }
  }

  /**
   * Stop cleanup timer
   */
  stop() {
    if (this.cleanupTimer) {
      clearInterval(this.cleanupTimer);
      this.cleanupTimer = null;
    }
  }
}

// Export singleton instance
module.exports = new PacketDeduplicationService();