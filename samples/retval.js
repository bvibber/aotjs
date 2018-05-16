function work() {
  return "work";
}

function play() {
  return "play";
}

var life = work() + play();

// should say "workplay";
console.log("should say 'workplay':", life);
