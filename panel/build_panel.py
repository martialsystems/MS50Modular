# Geometry lives in panel/assets/layout.json. Refresh the editor SVG with
# panel/emit_panel_svg.py. Re-run this file only when the module list should change.
import math,json,io,random,os,re
import numpy as np,cairosvg
from PIL import Image,ImageFont,ImageDraw
from scipy.ndimage import gaussian_filter
random.seed(5); rng=np.random.default_rng(5)
FP="/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
FT,FL,LST,LSL=12,11,.8,.4
fm={s:ImageFont.truetype(FP,s*8) for s in (FT,FL)}
tw=lambda s,z:fm[z].getlength(s)/8+(LST if z==FT else LSL)*(len(s)-1)
W,H,SC,M=1600,640,2,14; AVAIL=W-2*M
CH_Y=12; TI_Y=50; KT=78; KB=200; MINP=37; MAXP=74   # jack pitch: min fits hole+name+gap, max = 2x min
KR=13; RING=21; JR=9; PL=10; PITCH=50; KSP=56; INK="#dcd6c2"; GOLD="#c29f4c"
mods=[
 dict(t="VCO",kn=["RANGE","FINE","PW","FM 1","FM 2"],kc=2,J=["HZ/V","V/OCT","FM 1","FM 2","PWM","TRI","SAW","PULSE"]),
 dict(t="VCF",kn=["CUTOFF","PEAK","MOD"],J=["IN","CUTOFF","OUT"]),
 dict(t="VCA 1",kn=["INITIAL","MOD","LOW CUT"],J=["IN","ENV","OUT"]),
 dict(t="VCA 2",kn=["INITIAL","MOD"],J=["IN","CV","OUT"]),
 dict(t="MG",kn=["RATE","PW"],J=["FM","PWM","TRI","SAW","INV SAW","PULSE"]),
 dict(t="EG 1",kn=["ATTACK","DECAY","SUSTAIN","RELEASE"],J=["TRIG","OUT A","OUT B","OUT C"]),
 dict(t="EG 2",kn=["HOLD","DELAY","ATTACK","RELEASE"],J=["TRIG","OUT +","OUT \u2212","DELAY"]),
 dict(t="NOISE",J=["WHITE","PINK"]),
 dict(t="S&H",kn=["RATE"],J=["IN","OUT","CLOCK"]),
 dict(t="RING",J=["A","B","OUT"]),
 dict(t="DIV",sw=["2","4","16"],J=["IN","/2","/4","/16"]),
 dict(t="INV",J=["IN","OUT"]),
 dict(t="INT",kn=["TIME"],J=["IN","OUT"]),
 dict(t="MIX",kn=["LEVEL 1","LEVEL 2","LEVEL 3"],J=["IN 1","IN 2","IN 3","OUT"]),
 dict(t="MTR",meter=1),
 dict(t="EXT IN",J=["L","R","MONO","GATE"]),
 dict(t="OUTPUT",kn=["MIX","LEVEL"],J=["L","R","WET"]),
]
def jstart(m):                                  # jacks start under the column's own controls, or right under the title if it has none
    if m.get("kn"): return KT+23+(math.ceil(len(m["kn"])/m.get("kc",1))-1)*PITCH+KR+12+10
    if m.get("sw"): return KT+51
    return TI_Y+28
JB=max(jstart(m)+len(m["J"])*MINP for m in mods if m.get("J")); FR_T=TI_Y-6; FR_B=JB+6   # below FR_B is the cable lane
def need(m):
    jw=max(2*JR+14,max(tw(x,FL) for x in m["J"])+12) if m.get("J") else 0
    kw=0
    if m.get("kn"): kw=max(2*RING+(m.get("kc",1)-1)*KSP+10,max(tw(x,FL) for x in m["kn"])+10)
    if m.get("sw"): kw=2*(23*math.sin(math.radians(48))+tw("16",FL)/2)+8
    if m.get("meter"): kw=max(46,tw("VOLTS",FL)+10)
    return max(jw,kw,tw(m["t"],FT)+10)
for m in mods: m["need"]=need(m); m["w"]=m["need"]
spare=AVAIL-sum(m["w"] for m in mods); assert spare>=0,spare
for m in mods: m["w"]+=spare/len(mods)
P=[];texts=[];circ=[];K=[];J=[];CUR=[None,None]
def addtext(x0,y,s,z): w=tw(s,z); texts.append((x0,y-z*.74,x0+w,y+z*.2,s,CUR[0],CUR[1])); return w
def T(cx,y,s,z):
    w=addtext(cx-tw(s,z)/2,y,s,z); x0=cx-w/2
    P.append(f'<text x="{x0:.1f}" y="{y}" font-size="{z}" font-weight="700" letter-spacing="{LST if z==FT else LSL}" fill="{INK}">{s.replace("&","&amp;")}</text>')
def Lft(x0,y,s,z):
    addtext(x0,y,s,z); P.append(f'<text x="{x0:.1f}" y="{y}" font-size="{z}" font-weight="700" letter-spacing="{LSL}" fill="{INK}">{s.replace("&","&amp;")}</text>')
def knob(sec,lab,cx,cy,v,r=KR,label=True):
    if label:
        K.append(dict(section=sec,label=lab,cx=round(cx,1),cy=cy,radius=r,default=v,hit=[round(cx-RING,1),cy-RING,2*RING,2*RING])); circ.append((cx,cy,RING,"knob "+lab)); T(cx,cy+r+12,lab,FL)
        for i in range(11):
            a=math.radians(-135+27*i-90); l=4 if i%5==0 else 2.5
            P.append(f'<line x1="{cx+(r+4)*math.cos(a):.1f}" y1="{cy+(r+4)*math.sin(a):.1f}" x2="{cx+(r+4+l)*math.cos(a):.1f}" y2="{cy+(r+4+l)*math.sin(a):.1f}" stroke="{INK}" stroke-width="1.1"/>')
    a=math.radians(-135+270*v-90); ex,ey=math.cos(a),math.sin(a)
    P.append('<g class="kbody">'+f'<circle cx="{cx+1}" cy="{cy+2}" r="{r+4}" fill="#000" opacity=".45"/><circle cx="{cx}" cy="{cy}" r="{r+3}" fill="#08080a" stroke="#000" stroke-width=".8"/>'
      f'<circle cx="{cx}" cy="{cy}" r="{r+1.2}" fill="none" stroke="#3c3c3f" stroke-width="2.4" stroke-dasharray=".8 1.6" opacity=".75"/>'
      f'<circle cx="{cx}" cy="{cy}" r="{r-.3}" fill="url(#kb)" stroke="#000" stroke-width=".8"/><circle cx="{cx}" cy="{cy}" r="{r*.8:.1f}" fill="url(#kt)" stroke="#050505" stroke-width=".8"/>'
      f'<ellipse cx="{cx-r*.28:.1f}" cy="{cy-r*.32:.1f}" rx="{r*.45:.1f}" ry="{r*.28:.1f}" fill="url(#ks)" transform="rotate(-35 {cx} {cy})"/>'
      f'<line x1="{cx+ex*r*.1:.1f}" y1="{cy+ey*r*.1:.1f}" x2="{cx+ex*(r-1.5):.1f}" y2="{cy+ey*(r-1.5):.1f}" stroke="#f1ede0" stroke-width="2.4"/></g>')
def jack(sec,lab,hx,cy):                       # hole centered in column, name centered below it
    J.append(dict(section=sec,label=lab,x=round(hx,1),y=cy,radius=JR,hit=[round(hx-11,1),round(cy-11,1),22,22])); circ.append((hx,cy,JR+1.5,"jack "+lab))
    P.append(f'<circle cx="{hx:.1f}" cy="{cy}" r="{JR+1.5}" fill="#000" opacity=".55"/><circle cx="{hx:.1f}" cy="{cy}" r="{JR}" fill="url(#js)" stroke="#2a2a2c" stroke-width=".9"/>'
      f'<circle cx="{hx:.1f}" cy="{cy}" r="{JR-2.6}" fill="url(#jn)"/><circle cx="{hx:.1f}" cy="{cy}" r="{JR-5}" fill="#030303"/>')
    T(hx,round(cy+JR+13,1),lab,FL)
x=M; cols=[]
for m in mods:
    cx=x+m["w"]/2; sec=m["t"]; CUR[:]=[x+3,x+m["w"]-3]; cols.append(dict(title=m["t"],x=round(x,1),w=round(m["w"],1)))
    T(cx,TI_Y+15,m["t"],FT)
    if m.get("kn"):
        n=len(m["kn"]); kc=m.get("kc",1)
        for j,l in enumerate(m["kn"]):
            r,c=divmod(j,kc); cnt=min(kc,n-r*kc); knob(sec,l,cx+(c-(cnt-1)/2)*KSP,KT+23+r*PITCH,[.5,.3,.68,.42,.78][j%5])
    if m.get("sw"):
        cy=KT+27; circ.append((cx,cy,13,"switch")); K.append(dict(section=sec,label="RATIO SWITCH",cx=round(cx,1),cy=cy,radius=11,default=.5,hit=[round(cx-22),cy-22,44,44]))
        for lab,dg in zip(m["sw"],(-48,0,48)): T(cx+23*math.sin(math.radians(dg)),cy-23*math.cos(math.radians(dg))+4,lab,FL)
        knob(sec,"",cx,cy,.5,r=11,label=False)
    if m.get("meter"):
        P.append(f'<rect x="{cx-22:.1f}" y="{KT+4}" width="44" height="32" rx="3" fill="#050505"/><rect x="{cx-19:.1f}" y="{KT+7}" width="38" height="26" rx="2" fill="url(#mg)"/><line x1="{cx:.1f}" y1="{KT+31}" x2="{cx-8:.1f}" y2="{KT+11}" stroke="#111" stroke-width="1.5"/><circle cx="{cx:.1f}" cy="{KT+31}" r="2" fill="#111"/>')
        T(cx,KT+36+16,"VOLTS",FL)
    if m.get("J"):
        st=jstart(m); p=min(MAXP,(JB-st)/len(m["J"])); cols[-1].update(jackStart=st,jackPitch=round(p,1))
        for i,lab in enumerate(m["J"]): jack(sec,lab,cx,round(st+JR+1.5+i*p,1))
    x+=m["w"]
rules=[c["x"] for c in cols[1:]]
chrome=(f'<text x="{M+4}" y="{CH_Y+21}" font-size="14" font-weight="700" letter-spacing="3" fill="#9a9684">SYNTHESIZER</text>'
  f'<text x="{W-M-228}" y="{CH_Y+19}" font-size="{FL}" font-weight="700" letter-spacing="{LSL}" fill="{INK}">MIX</text><rect x="{W-M-190}" y="{CH_Y+13}" width="170" height="4" rx="2" fill="#050505" stroke="#3a3a3c"/>'
  f'<rect x="{W-M-190}" y="{CH_Y+13}" width="119" height="4" rx="2" fill="{GOLD}" opacity=".8"/><rect x="{W-M-76}" y="{CH_Y+5}" width="12" height="20" rx="2" fill="url(#kb)" stroke="#000"/><line x1="{W-M-70}" y1="{CH_Y+8}" x2="{W-M-70}" y2="{CH_Y+22}" stroke="#f1ede0" stroke-width="1.6"/>')
svg=(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="Liberation Sans"><defs>'
 '<linearGradient id="pf" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#242426"/><stop offset="1" stop-color="#161618"/></linearGradient>'
 '<radialGradient id="js" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#e6e6e1"/><stop offset="1" stop-color="#7d7d79"/></radialGradient><radialGradient id="jn" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#9a9a96"/><stop offset="1" stop-color="#3c3c3c"/></radialGradient>'
 '<linearGradient id="kb" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#4b4b4e"/><stop offset=".5" stop-color="#1a1a1b"/><stop offset="1" stop-color="#060607"/></linearGradient><linearGradient id="kt" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#2a2a2c"/><stop offset="1" stop-color="#131314"/></linearGradient>'
 '<radialGradient id="ks"><stop offset="0" stop-color="#fff" stop-opacity=".16"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient><radialGradient id="mg" cx=".5" cy=".9" r="1"><stop offset="0" stop-color="#f3dc92"/><stop offset="1" stop-color="#c9983a"/></radialGradient></defs>'
 f'<rect width="{W}" height="{H}" rx="8" fill="url(#pf)"/><rect x="3" y="3" width="{W-6}" height="{H-6}" rx="6" fill="none" stroke="#050506" stroke-width="2"/>'
 f'<rect x="{M}" y="{FR_T}" width="{AVAIL}" height="{FR_B-FR_T}" fill="none" stroke="{GOLD}" stroke-width="1.6"/>'
 +"".join(f'<line x1="{r}" y1="{FR_T}" x2="{r}" y2="{FR_B}" stroke="{GOLD}" stroke-width="1.6"/>' for r in rules)
 +chrome+"".join(P)
 +"".join(f'<g transform="translate({sx} {sy}) rotate({random.randint(0,90)})"><circle r="5" fill="url(#js)" stroke="#000" stroke-width=".9"/><line x1="-3.5" x2="3.5" stroke="#1a1a1a" stroke-width="1.5"/><line y1="-3.5" y2="3.5" stroke="#1a1a1a" stroke-width="1.5"/></g>' for sx,sy in((9,9),(W-9,9),(9,H-9),(W-9,H-9)))+'</svg>')
# ---- verification ----
bad=[]
for i,a in enumerate(texts):
    if a[5] is not None and not (a[5]<=a[0] and a[2]<=a[6]): bad.append(("text outside column",a[4]))
    for b in texts[i+1:]:
        if a[0]<b[2]+2 and b[0]<a[2]+2 and a[1]<b[3]+1 and b[1]<a[3]+1: bad.append(("text/text",a[4],b[4]))
    for cx,cy,r,n in circ:
        px=min(max(cx,a[0]),a[2]); py=min(max(cy,a[1]),a[3])
        if (px-cx)**2+(py-cy)**2<(r-(5 if n.startswith("knob") else 0))**2: bad.append(("text/ctl",a[4],n))
    for r in rules+[M,W-M]:
        if a[0]-2<r<a[2]+2 and a[3]>FR_T and a[1]<FR_B: bad.append(("text/rule",a[4],r))
for cx,cy,r,n in circ:
    for rr in rules+[M,W-M]:
        if abs(cx-rr)<r+1: bad.append(("ctl/rule",n,rr))
    if cy+r>FR_B: bad.append(("control below frame",n))
hh=[h["hit"] for h in J]+[h["hit"] for h in K]
for i,a in enumerate(hh):
    for b in hh[i+1:]:
        if a[0]<b[0]+b[2] and b[0]<a[0]+a[2] and a[1]<b[1]+b[3] and b[1]<a[1]+a[3]: bad.append(("hit/hit",a,b))
exp=dict(knobs=31,jacks=58)
print(f"canvas {W}x{H} ratio {W/H:.2f} | frame y {FR_T}-{FR_B}, lane y {FR_B}-{H-M} ({H-M-FR_B}px) | spare {spare:.0f}px (+{spare/len(mods):.1f}/col)")
print("widths:",{m["t"]:round(m["w"]) for m in mods},"| needs sum",round(sum(m["need"] for m in mods)),"of",AVAIL)
print("knobs",len(K),"(incl. switch) jacks",len(J),"expected",exp,"| sizes",{FT,FL},"| overlaps:",bad)
assert len(K)==exp["knobs"] and len(J)==exp["jacks"]
# ---- finish ----
blur=lambda a,r:gaussian_filter(a.astype(np.float32),r)
ss=lambda a,b,x:(lambda t:t*t*(3-2*t))(np.clip((x-a)/(b-a),0,1))
def fbm(h,w,oc=6,base=4,seed=0):
    r=np.random.default_rng(seed); acc=np.zeros((h,w),np.float32); amp=1.;tot=0
    for o in range(oc):
        n=base*2**o; a=np.ascontiguousarray(r.random((max(2,n*h//w),n)).astype(np.float32)); acc+=amp*np.asarray(Image.fromarray(a).resize((w,h),Image.BICUBIC)); tot+=amp; amp*=.5
    acc/=tot; return (acc-acc.min())/(acc.max()-acc.min())
def finish(svg):
    global rng
    random.seed(5); rng=np.random.default_rng(5)   # same wear on both renders
    a=np.asarray(Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(),output_width=W*SC,output_height=H*SC))).convert("RGB")).astype(np.float32)/255; h,w=a.shape[:2]
    paint=ss(.38,.5,a.mean(-1)); a=a*(1-paint[...,None])+a*np.array([1,.96,.88],np.float32)*(.96+.04*fbm(h,w,5,6,1))[...,None]*paint[...,None]
    a+=(1-paint)[...,None]*((rng.random((h,w,1))-.5).astype(np.float32)*.05+(blur(rng.random((h,w)),.9)[...,None]-.5)*.1)
    a*=(.9+.2*fbm(h,w,6,3,2))[...,None]
    yy,xx=np.mgrid[0:h,0:w].astype(np.float32); d=np.minimum(np.minimum(xx,w-xx),np.minimum(yy,h-yy))
    wm=np.exp(-d/(14*SC))*np.clip((fbm(h,w,5,20,4)-.5)*3.5,0,1)*.4; a=a*(1-.5*wm[...,None])+np.array([.36,.36,.34],np.float32)*.5*wm[...,None]
    sl=Image.new("L",(w,h),0); dr=ImageDraw.Draw(sl)
    for _ in range(40):
        x0,y0=random.uniform(0,w),random.uniform(0,h); L=random.uniform(10,120)*SC; t=random.uniform(0,6.28); dr.line([(x0,y0),(x0+L*math.cos(t),y0+L*math.sin(t))],fill=random.randint(40,110),width=1)
    a+=blur(np.asarray(sl).astype(np.float32)/255,.6)[...,None]*.14
    a+=blur((rng.random((h,w))>.9996).astype(np.float32),1.0)[...,None]*1.5
    r=np.sqrt(((xx/w-.5)/.5)**2+((yy/h-.5)/.5)**2)/1.414; a*=(1-.28*r**2.5)[...,None]; a*=np.array([1.03,1,.95],np.float32); a+=.012
    return Image.fromarray((np.clip(a,0,1)**.97*255).astype(np.uint8))
o=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","assets")+"/"; os.makedirs(o,exist_ok=True)
im=finish(svg); im.save(o+"panel@2x.png"); im.resize((W,H),Image.LANCZOS).save(o+"panel.png")
bg=finish(re.sub(r'<g class="kbody">.*?</g>',"",svg)); bg.save(o+"panel_bg@2x.png"); bg.resize((W,H),Image.LANCZOS).save(o+"panel_bg.png")   # no knob bodies: for live knobs
json.dump(dict(canvas=[W,H],bands=dict(chrome=[CH_Y,30],titles=[TI_Y,20],knobs=[KT,KB],lane=[FR_B,H-M-FR_B]),columns=cols,knobs=K,jacks=J,labels=[dict(text=t[4],rect=[round(t[0],1),round(t[1],1),round(t[2]-t[0],1),round(t[3]-t[1],1)]) for t in texts]),open(o+"layout.json","w"),indent=1)
