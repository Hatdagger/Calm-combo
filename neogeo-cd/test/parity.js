// Runs the same scripted game using the rule functions copied verbatim from web/index.html.
const fs=require('fs');
const html=fs.readFileSync(process.argv[2],'utf8');
const grab=name=>{const i=html.indexOf('function '+name+'(');let d=0,j=html.indexOf('{',i);for(;;j++){if(html[j]==='{')d++;else if(html[j]==='}'&&--d===0)break}return html.slice(i,j+1)};
const COLS=8,ROWS=7,NC=6;
const F=new Function('COLS','ROWS','NC',grab('mulberry32')+grab('findMatches')+grab('applyMove')+grab('hasValidMove')+';return {mulberry32,findMatches,applyMove,hasValidMove}')(COLS,ROWS,NC);
const {mulberry32,findMatches,applyMove,hasValidMove}=F;
const seed=+process.argv[3], turns=+process.argv[4];
const rng=mulberry32(seed), rnd=()=>Math.floor(rng()*NC);
let g, tries=0;
do{g=[];for(let r=0;r<ROWS;r++){const row=[];for(let c=0;c<COLS;c++){let k;do{k=rnd()}while((c>=2&&row[c-1]===k&&row[c-2]===k)||(r>=2&&g[r-1][c]===k&&g[r-2][c]===k));row.push(k)}g.push(row)}}while(!hasValidMove(g)&&++tries<50);
let score=0,moves=15;
const dump=()=>console.log(g.map(r=>r.join('')).join('')+' '+score+' '+moves);
dump();
for(let i=0;i<turns&&moves>0;i++){
  let mv=null;
  outer: for(let r=0;r<ROWS;r++)for(let c=0;c<COLS;c++){
    for(let t=0;t<COLS;t++)if(t!==c&&findMatches(applyMove(g,r,c,'h',t)).count){mv=[r,c,'h',t];break outer}
    for(let t=0;t<ROWS;t++)if(t!==r&&findMatches(applyMove(g,r,c,'v',t)).count){mv=[r,c,'v',t];break outer}
  }
  if(!mv){console.log('nomove');break}
  const [r,c,axis,t]=mv, preview=findMatches(applyMove(g,r,c,axis,t));
  g=applyMove(g,r,c,axis,t);
  const fr=axis==='h'?r:t, fc=axis==='h'?t:c, lever=!preview.hit[fr][fc];
  moves--; let chain=0,gain=0;
  for(;;){
    const m=findMatches(g); if(!m.count)break; chain++; let pts=0;
    for(const n of m.groups){pts+=n*10+(n===4?30:n>=5?80:0);if(n===4)gain+=1;else if(n>=5)gain+=2}
    if(chain>=2)gain+=1; if(chain===1&&lever)pts+=50; pts*=chain; score+=pts;
    const dying=m.hit.map(r=>r.slice());
    for(let r=0;r<ROWS;r++)for(let c=0;c<COLS;c++)if(m.hit[r][c])for(const[dr,dc]of[[1,0],[-1,0],[0,1],[0,-1]]){const r2=r+dr,c2=c+dc;if(r2>=0&&r2<ROWS&&c2>=0&&c2<COLS&&g[r2][c2]===6)dying[r2][c2]=true}
    for(let c=0;c<COLS;c++){const keep=[];for(let r=ROWS-1;r>=0;r--)if(!dying[r][c])keep.push(g[r][c]);let r=ROWS-1;for(const k of keep)g[r--][c]=k;const n=r+1;for(let k=0;k<n;k++)g[k][c]=rnd()}
  }
  moves+=gain; dump();
  if(!hasValidMove(g)){console.log('shuffle');break}
}
