(function(root){'use strict';
const aliases={h:'help',a:'about',c:'cpu',m:'mem',t:'selftest',l:'ls',r:'cat',w:'write',n:'new',d:'rm',b:'alloc',e:'echo',x:'clear',q:'halt',lf:'ls',rf:'cat',df:'rm',st:'selftest'};
const descriptions={help:'Show every command',about:'About this kernel',cpu:'CPU information',mem:'Memory usage',selftest:'Run built-in tests',ls:'List RAM files',cat:'Read a RAM file',write:'Write or replace a RAM file',new:'Create a RAM file without overwriting',rm:'Remove a RAM file',alloc:'Allocate bytes',echo:'Print text',clear:'Clear the guest display',halt:'Halt the guest CPU'};
const examples=['show my files','make file','read welcome.txt','show memory','run tests','what can I do','h','l','r welcome.txt','n note.txt hello','b 128'];
function distance(a,b){const d=Array.from({length:a.length+1},()=>Array(b.length+1).fill(0));for(let i=0;i<=a.length;i++)d[i][0]=i;for(let j=0;j<=b.length;j++)d[0][j]=j;for(let i=1;i<=a.length;i++)for(let j=1;j<=b.length;j++){d[i][j]=Math.min(d[i-1][j]+1,d[i][j-1]+1,d[i-1][j-1]+(a[i-1]!==b[j-1]));if(i>1&&j>1&&a[i-1]===b[j-2]&&a[i-2]===b[j-1])d[i][j]=Math.min(d[i][j],d[i-2][j-2]+1);}return d[a.length][b.length];}
function suggestions(value){const input=value.trim().toLowerCase();if(!input)return examples.slice(0,5);let choices=examples.concat(Object.keys(descriptions));return [...new Set(choices)].sort((a,b)=>{const score=s=>(s.startsWith(input)?-100:s.includes(input)?-50:distance(input,s));return score(a)-score(b)}).slice(0,5);}
function command(text){const first=text.split(' ')[0];return {kind:'command',command:text,description:descriptions[first]||'Kernel command'};}
function resolve(input){const original=input.trim(),lower=original.toLowerCase();if(!original)return {kind:'empty'};
 let match;
 if(/^(make|create)( a)? file$/.test(lower)||['f','mf','new file'].includes(lower))return {kind:'create',name:''};
 if((match=original.match(/^(?:make|create)(?: a)? file\s+([A-Za-z0-9_.-]+)$/i)))return {kind:'create',name:match[1]};
 if((match=original.match(/^run(?:\s+(.*))?$/i))){const program=(match[1]||'').toLowerCase();const programs={help:'help',about:'about',cpu:'cpu',memory:'mem',mem:'mem',tests:'selftest',selftest:'selftest',files:'ls',ls:'ls',notepad:'@create',notes:'@create'};if(programs[program])return programs[program]==='@create'?{kind:'create',name:''}:command('run '+programs[program]);return {kind:'unsupported',message:'No external executable loader yet. Choose a supported built-in utility or the browser notepad.',suggestions:Object.keys(programs).sort((a,b)=>distance(program,a)-distance(program,b)).slice(0,4).map(x=>'run '+x)};}
 const phrases={'show help':'help','what can i do':'help','what can i do?':'help','help me':'help','about this os':'about','what is this':'about','show files':'ls','show my files':'ls','list files':'ls','list my files':'ls','what files do i have':'ls','show memory':'mem','memory usage':'mem','show cpu':'cpu','cpu info':'cpu','run tests':'selftest','check system':'selftest','test':'selftest','clear screen':'clear','clear terminal':'clear'};
 if(phrases[lower])return command(phrases[lower]);
 // Approximate whole phrases only when the resulting operation is read-only.
 const safePhrases=Object.keys(phrases).filter(p=>['help','about','cpu','mem','selftest','ls'].includes(phrases[p]));
 const near=safePhrases.filter(p=>lower.length>=6&&distance(lower,p)<=Math.min(2,Math.floor(lower.length/6)));
 const intents=[...new Set(near.map(p=>phrases[p]))];
 if(intents.length===1)return {...command(intents[0]),correction:original};

 if((match=original.match(/^(?:read|show file|open)\s+([A-Za-z0-9_.-]+)$/i)))return command('cat '+match[1]);
 if((match=original.match(/^(?:delete|remove)(?: file)?\s+([A-Za-z0-9_.-]+)$/i)))return command('rm '+match[1]);
 if((match=original.match(/^(?:save|write)\s+(.+)\s+(?:in|to)\s+([A-Za-z0-9_.-]+)$/i)))return command('write '+match[2]+' '+match[1]);
 if((match=original.match(/^(\S+)(?:\s+(.*))?$/))){const first=match[1].toLowerCase(),rest=match[2]?' '+match[2]:'';if(aliases[first])return command(aliases[first]+rest);if(descriptions[first])return command(first+rest);const safe=['help','about','cpu','mem','selftest'];const matches=safe.filter(x=>first.length>=3&&distance(first,x)===1);if(matches.length===1)return {...command(matches[0]+rest),correction:match[1]};}
 return {kind:'unsupported',message:'I do not recognize that phrase. Choose a suggestion or use a visible command.',suggestions:suggestions(original)};
}
function nextFilename(files){const taken=new Set(files.map(f=>typeof f==='string'?f:f.name));for(let n=1;n<10000;n++){const candidate='file'+n+'.txt';if(!taken.has(candidate))return candidate;}return 'note.txt';}
const api={aliases,descriptions,examples,resolve,suggestions,nextFilename};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.OriginCommands=api;
})(typeof window!=='undefined'?window:this);
