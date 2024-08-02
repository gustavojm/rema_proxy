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