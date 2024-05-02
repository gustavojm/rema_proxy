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