/**
 * Utility library functions
 */

/**
 * Generate a unique identifier
 * @returns {string} Unique identifier
 */
function uid() {
  return Math.random().toString(36).substring(2, 15) + 
         Math.random().toString(36).substring(2, 15);
}

/**
 * Create a console formatter for consistent output
 * @param {number} width - Column width for alignment
 * @returns {Function} Formatter function
 */
function createFormatter(width = 20) {
  return function(label, message) {
    const padding = ' '.repeat(Math.max(0, width - label.length));
    console.log(`${label}:${padding}${message}`);
  };
}

module.exports = {
  uid,
  createFormatter
};