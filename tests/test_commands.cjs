const assert=require('node:assert/strict'),C=require('../browser-site/dist/command-help.js');
assert.equal(C.resolve('show my files').command,'ls');assert.equal(C.resolve('r MixedCase.TXT').command,'cat MixedCase.TXT');
assert.equal(C.resolve('write MyFile.TXT Exact Case Payload').command,'write MyFile.TXT Exact Case Payload');
assert.equal(C.resolve('hlep').command,'help');assert.equal(C.resolve('hlep').correction,'hlep');
assert.equal(C.resolve('mr note.txt').kind,'unsupported');assert.equal(C.resolve('delet note.txt').kind,'unsupported');
assert.equal(C.resolve('make file').kind,'create');assert.equal(C.resolve('f').kind,'create');
assert.equal(C.resolve('make file Test.TXT').name,'Test.TXT');assert.equal(C.resolve('run edge.exe').kind,'unsupported');
assert.equal(C.resolve('run selftest').command,'run selftest');assert.equal(C.resolve('run tsts').suggestions[0],'run tests');
assert.equal(C.nextFilename([{name:'file1.txt'},{name:'file3.txt'}]),'file2.txt');
assert.equal(C.resolve('save Hello, World! in Hello.TXT').command,'write Hello.TXT Hello, World!');
assert.equal(C.resolve('delete Hello.TXT').command,'rm Hello.TXT');
assert.equal(C.resolve('install malware').kind,'unsupported');
assert.equal(C.resolve('shwo my files').command,'ls');
assert.equal(C.resolve('show memroy').command,'mem');
assert.equal(C.resolve('what can i doo').command,'help');
assert.equal(C.resolve('delete file Mixed.TXT').command,'rm Mixed.TXT');
assert.equal(C.resolve('remove Mixed.TXT').command,'rm Mixed.TXT');
assert.equal(C.resolve('rm Mixed.TXT').command,'rm Mixed.TXT');
assert.equal(C.resolve('removee Mixed.TXT').kind,'unsupported');
assert.equal(C.resolve('makee file').kind,'unsupported');
assert.equal(C.resolve('save delte this in note.txt').command,'write note.txt delte this');
console.log('PASS: 26 command/alias/typo/payload/suggestion/filename safety assertions.');

assert.equal(C.resolve('train hello world').command,'hello');assert.equal(C.resolve('i').command,'hello');assert.equal(C.resolve('stop training').command,'stop');assert.equal(C.resolve('g').command,'generate');assert.equal(C.resolve('o Mixed.txt').command,'edit Mixed.txt');assert.equal(C.resolve('run hello').command,'run hello');console.log('PASS: 6 guest training/editor command checks.');

assert.equal(C.resolve('run Demo.fo').command,'run Demo.fo');assert.equal(C.resolve('code print 2+3').command,'code print 2+3');console.log('PASS: Origin Script bridge keeps program name and source intact.');
