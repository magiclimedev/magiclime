const EventEmitter = require('events');
const config = require('./config');
const logger = require('./logger');

/**
 * Efficient queue manager using event-driven architecture
 */
class QueueManager extends EventEmitter {
  constructor() {
    super();
    this.queues = {
      incoming: [],    // Incoming serial data
      retry: [],       // Failed forwarding attempts
      priority: []     // High priority messages
    };
    
    this.processing = {
      incoming: false,
      retry: false
    };
    
    this.stats = {
      processed: 0,
      failed: 0,
      retried: 0,
      dropped: 0
    };
  }
  
  /**
   * Add item to queue
   */
  enqueue(item, queueName = 'incoming') {
    const queue = this.queues[queueName];
    if (!queue) {
      logger.error('Invalid queue name', { queueName });
      return false;
    }
    
    // Check queue size limit
    if (queue.length >= config.queue.maxSize) {
      // Drop oldest item
      const dropped = queue.shift();
      this.stats.dropped++;
      logger.warn('Queue full, dropping oldest item', { 
        queue: queueName, 
        droppedUid: dropped?.uid 
      });
    }
    
    // Ensure item is an object and add timestamp if not present
    let queueItem;
    if (typeof item === 'string') {
      // Convert string to object with timestamp
      queueItem = {
        data: item,
        timestamp: Date.now()
      };
    } else if (typeof item === 'object' && item !== null) {
      // Clone object and add timestamp if not present
      queueItem = { ...item };
      if (!queueItem.timestamp) {
        queueItem.timestamp = Date.now();
      }
    } else {
      // Fallback for other types
      queueItem = {
        data: item,
        timestamp: Date.now()
      };
    }
    
    queue.push(queueItem);
    
    // Emit event for immediate processing
    setImmediate(() => {
      this.emit(`${queueName}:ready`);
    });
    
    return true;
  }
  
  /**
   * Dequeue item from queue
   */
  dequeue(queueName = 'incoming') {
    const queue = this.queues[queueName];
    return queue ? queue.shift() : null;
  }
  
  /**
   * Peek at next item without removing
   */
  peek(queueName = 'incoming') {
    const queue = this.queues[queueName];
    return queue ? queue[0] : null;
  }
  
  /**
   * Get queue size
   */
  size(queueName = 'incoming') {
    const queue = this.queues[queueName];
    return queue ? queue.length : 0;
  }
  
  /**
   * Check if queue is empty
   */
  isEmpty(queueName = 'incoming') {
    return this.size(queueName) === 0;
  }
  
  /**
   * Move failed item to retry queue
   */
  addToRetry(item) {
    // Increment retry count
    item.retryCount = (item.retryCount || 0) + 1;
    item.lastRetry = Date.now();
    
    // Check retry limit
    if (item.retryCount > config.queue.maxRetries) {
      this.stats.dropped++;
      logger.warn('Max retries exceeded, dropping item', {
        uid: item.uid,
        retries: item.retryCount
      });
      return false;
    }
    
    // Check item age
    const age = Date.now() - item.timestamp;
    if (age > config.queue.itemTTL) {
      this.stats.dropped++;
      logger.warn('Item too old, dropping', {
        uid: item.uid,
        age: Math.round(age / 1000) + 's'
      });
      return false;
    }
    
    return this.enqueue(item, 'retry');
  }
  
  /**
   * Get items ready for retry
   */
  getRetryItems() {
    const now = Date.now();
    const readyItems = [];
    const notReady = [];
    
    // Check each item in retry queue
    while (!this.isEmpty('retry')) {
      const item = this.dequeue('retry');
      
      const timeSinceRetry = now - (item.lastRetry || item.timestamp);
      if (timeSinceRetry >= config.queue.retryDelay) {
        readyItems.push(item);
      } else {
        notReady.push(item);
      }
    }
    
    // Put not-ready items back
    this.queues.retry = notReady;
    
    return readyItems;
  }
  
  /**
   * Clear old items from all queues
   */
  cleanup() {
    const now = Date.now();
    let cleaned = 0;
    
    for (const [queueName, queue] of Object.entries(this.queues)) {
      const before = queue.length;
      
      this.queues[queueName] = queue.filter(item => {
        const age = now - item.timestamp;
        if (age > config.queue.itemTTL) {
          cleaned++;
          return false;
        }
        return true;
      });
      
      const after = this.queues[queueName].length;
      if (before !== after) {
        logger.debug('Queue cleanup', {
          queue: queueName,
          removed: before - after
        });
      }
    }
    
    if (cleaned > 0) {
      this.stats.dropped += cleaned;
    }
    
    return cleaned;
  }
  
  /**
   * Get queue statistics
   */
  getStats() {
    const queueSizes = {};
    for (const [name, queue] of Object.entries(this.queues)) {
      queueSizes[name] = queue.length;
    }
    
    return {
      queues: queueSizes,
      stats: this.stats,
      processing: this.processing
    };
  }
  
  /**
   * Reset statistics
   */
  resetStats() {
    this.stats = {
      processed: 0,
      failed: 0,
      retried: 0,
      dropped: 0
    };
  }
  
  /**
   * Clear all queues
   */
  clear() {
    for (const queue of Object.values(this.queues)) {
      queue.length = 0;
    }
    this.resetStats();
  }
}

module.exports = QueueManager;