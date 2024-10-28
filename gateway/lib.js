module.exports = {

  // return TRUE if the string is ASCII
  isASCII: function (str) {
    return /^[\x00-\x7F]*$/.test(str);
  },

  convertTimestamp: function (timestamp) {
    var d = new Date(timestamp * 1000),	// Convert the passed timestamp to milliseconds
      yyyy = d.getFullYear(),
      mm = ('0' + (d.getMonth() + 1)).slice(-2),	// Months are zero based. Add leading 0.
      dd = ('0' + d.getDate()).slice(-2),			// Add leading 0.
      hh = ('0' + d.getHours()).slice(-2),
      h = hh,
      min = ('0' + d.getMinutes()).slice(-2),		// Add leading 0.
      sec = ('0' + d.getSeconds()).slice(-2),   // Add leading 0.
      ampm = 'AM',
      time;
        
    time = yyyy + '-' + mm + '-' + dd + ' ' + h + ':' + min + ':' + sec;
      
    return time;
  },

  createFormatter: function(initialModuleLength = 0) {
    let maxModuleLength = initialModuleLength;
  
    return function(module, message) {
        // Update max module length if the current module is longer
        maxModuleLength = Math.max(maxModuleLength, module.length);
  
        // Format output with updated padding
        const paddedModule = module.padEnd(maxModuleLength + 2, ' '); // extra 2 spaces for padding
        console.log(`${paddedModule}${message}`);
    };
  }
};
