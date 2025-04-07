class TwoWayMap {
    constructor(map) {
       this.map = map;
       this.reverseMap = {};
       for(const key in map) {
          const value = map[key];
          this.reverseMap[value] = key;   
       }
    }
    get(key) { return this.map[key]; }
    revGet(key) { return this.reverseMap[key]; }
}

function isEqual(value1, value2) {
   if (typeof value1 !== 'object' || value1 === null || typeof value2 !== 'object' || value2 === null) {
       return value1 === value2;
   }

   let keys1 = Object.keys(value1);
   let keys2 = Object.keys(value2);

   if (keys1.length !== keys2.length) return false;

   for (let key of keys1) {
       if (!isEqual(value1[key], value2[key])) {
           return false;
       }
   }

   return true;
}

function findDifferences(old_obj, new_obj, path = '') {
   let differences = [];

   if (typeof old_obj !== 'object' || old_obj === null || typeof new_obj !== 'object' || new_obj === null) {
       return differences;
   }

   let keys1 = Object.keys(old_obj);

   for (let key of keys1) {
       let newPath = path ? `${path}.${key}` : key;
       if (!isEqual(old_obj[key], new_obj[key])) {
           if (typeof old_obj[key] === 'object' && typeof new_obj[key] === 'object') {
               differences = differences.concat(findDifferences(old_obj[key], new_obj[key], newPath));
           } else {
               differences.push({[newPath]: new_obj[key]} );
           }
       }
   }

   return differences;
}

function findDifferencesShallow(old_obj, new_obj, path = '') {
    let differences = [];
 
    if (typeof old_obj !== 'object' || old_obj === null || typeof new_obj !== 'object' || new_obj === null) {
        return differences;
    }
 
    let keys1 = Object.keys(old_obj);
 
    for (let key of keys1) {
        let newPath = path ? `${path}.${key}` : key;
        if (!isEqual(old_obj[key], new_obj[key])) {
            differences.push({[newPath]: new_obj[key]} );
        }
    }
 
    return differences;
 }

 function replaceString(oldS, newS, fullS) {
    for (let i = 0; i < fullS.length; ++i) {
      if (fullS.substring(i, i + oldS.length) === oldS) {
        fullS =
          fullS.substring(0, i) +
          newS +
          fullS.substring(i + oldS.length, fullS.length);
      }
    }
    return fullS;
  }


// A map to store callbacks associated with unique request identifiers
const requestCallbacks = new Map();

function AjaxREMA(dataObject, successCallback, errorCallback) {
    // Generate a unique identifier for the request
    const request_id = Date.now();  // You can use a UUID or other method for generating unique IDs

    // Store the success and error callbacks in the map, associated with this request_id
    requestCallbacks.set(request_id, { success: successCallback, error: errorCallback });

    data = {
        request_id: request_id,
        payload: dataObject
    };

    // Send the AJAX request
    $.ajax({
        url: "/REMA",
        method: "POST",
        contentType : "application/json",
        dataType : "json",
        data: JSON.stringify(data),
        success: function(response) {
            const receivedRequestId = response.request_id;  // Get the request_id from the response
            
            if (requestCallbacks.has(receivedRequestId)) {
                // Retrieve the original callbacks
                const callbacks = requestCallbacks.get(receivedRequestId);

                // Execute the success callback if it exists
                if (callbacks.success) {
                    callbacks.success(response.payload);
                }

                // Remove the callback from the map since it's no longer needed
                requestCallbacks.delete(receivedRequestId);
            } else {
                console.warn("No callback found for Request ID:", receivedRequestId);
            }
        },
        error: function(jqXHR, textStatus, errorThrown) {
            // Handle errors using the request_id
            if (requestCallbacks.has(request_id)) {
                const callbacks = requestCallbacks.get(request_id);
                
                // Execute the error callback if it exists
                if (callbacks.error) {
                    callbacks.error(textStatus, errorThrown);
                }

                // Remove the callback from the map
                requestCallbacks.delete(request_id);
            } else {
                console.warn("No callback found for Request ID:", request_id);
            }
        }
    });
}

function preventFocus(event) {
    if (event.relatedTarget) {
      // Revert focus back to previous blurring element
      event.relatedTarget.focus();
    } else {
      // No previous focus target, blur instead
      this.blur();
      // Alternatively: event.currentTarget.blur();
    }
}

(function($) {
    
    $.ajaxREMA = function (ajaxOpts) {
        // Generate a unique identifier for the request
        const request_id = Date.now();  // You can use a UUID or other method for generating unique IDs

        // Store the success and error callbacks in the map, associated with this request_id
        requestCallbacks.set(request_id, { success: ajaxOpts.success, error: ajaxOpts.error });

        // Send the AJAX request
        $.ajax({
            url: "/REMA/" + request_id,
            method: "POST",
            contentType: ajaxOpts.contentType | "application/json",
            dataType: ajaxOpts.dataType | "json",
            data: JSON.stringify(ajaxOpts.data),
            success: function (response) {
                const receivedRequestId = response.request_id;  // Get the request_id from the response

                if (requestCallbacks.has(receivedRequestId)) {
                    // Retrieve the original callbacks
                    const callbacks = requestCallbacks.get(receivedRequestId);

                    // Execute the success callback if it exists
                    if (callbacks.success) {
                        callbacks.success(response.payload);
                    }

                    // Remove the callback from the map since it's no longer needed
                    requestCallbacks.delete(receivedRequestId);
                } else {
                    console.warn("No callback found for Request ID:", receivedRequestId);
                }
            },
            error: function (jqXHR, textStatus, errorThrown) {
                // Handle errors using the request_id
                if (requestCallbacks.has(request_id)) {
                    const callbacks = requestCallbacks.get(request_id);

                    // Execute the error callback if it exists
                    if (callbacks.error) {
                        callbacks.error(textStatus, errorThrown);
                    }

                    // Remove the callback from the map
                    requestCallbacks.delete(request_id);
                } else {
                    console.warn("No callback found for Request ID:", request_id);
                }
            }
        });
    }
})(jQuery);