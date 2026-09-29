import { useEffect, useRef, useState } from 'react'
import * as THREE from 'three'
import { ConvexGeometry } from 'three/addons/geometries/ConvexGeometry.js'
import gsap from 'gsap'
import { ScrollTrigger } from 'gsap/ScrollTrigger'
import Lenis from 'lenis'
import { stages } from '../data'
import NetworkJourney from './NetworkJourney'

const NAVY = 0x1b2a41
const BLUE = 0x4d82a7
const ORANGE = 0xe87f24
const WHITE = 0xf8fbff
const FADED_BLUE = new THREE.Color(0xc9d8e5)
const instanceDummy = new THREE.Object3D()
const point = (x: number, y: number, z = 0) => new THREE.Vector3(x, y, z)
const clamp = (n: number, a = 0, b = 1) => Math.max(a, Math.min(b, n))

function material(color: number, opacity = 1) {
  const result = new THREE.MeshBasicMaterial({ color, transparent: true, opacity, depthWrite: false, side: THREE.DoubleSide })
  result.userData.baseOpacity = opacity
  return result
}
function line(points: THREE.Vector3[], color: number, opacity = 1) {
  const geo = new THREE.BufferGeometry().setFromPoints(points)
  const mat = new THREE.LineBasicMaterial({ color, transparent: true, opacity, depthWrite: false })
  mat.userData.baseOpacity = opacity
  return new THREE.Line(geo, mat)
}
function instance(geometry: THREE.BufferGeometry, count: number, color: number, opacity = 1) {
  const mesh = new THREE.InstancedMesh(geometry, material(color, opacity), count)
  mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage)
  return mesh
}
function put(mesh: THREE.InstancedMesh, i: number, position: THREE.Vector3, size: number) {
  instanceDummy.position.copy(position)
  instanceDummy.scale.setScalar(size)
  instanceDummy.updateMatrix()
  mesh.setMatrixAt(i, instanceDummy.matrix)
}
function setOpacity(group: THREE.Group, weight: number) {
  group.visible = weight > .012
  if (!group.visible) return
  group.traverse(obj => {
    if (!(obj instanceof THREE.Mesh || obj instanceof THREE.Line || obj instanceof THREE.LineSegments)) return
    const m = obj.material as THREE.Material & { opacity?: number }
    if (typeof m.opacity === 'number') m.opacity = Number(m.userData.baseOpacity ?? 1) * weight
  })
}

function makePolytope() {
  const group = new THREE.Group()
  const vertices = [
    point(-1.65,-1.35,-.65),point(.25,-1.35,-.9),point(1.8,-.72,-.35),
    point(1.65,.8,-.7),point(.15,1.55,-.6),point(-1.65,.8,-.6),
    point(-1.15,-.95,1),point(.45,-.75,1.35),point(1.3,.35,1.1),point(-.35,1.25,1),
  ]
  const hull = new ConvexGeometry(vertices)
  const face = new THREE.Mesh(hull,material(BLUE,.15))
  group.add(face)
  const wireMat = new THREE.LineBasicMaterial({color:NAVY,transparent:true,opacity:.56,depthWrite:false})
  wireMat.userData.baseOpacity = .56
  group.add(new THREE.LineSegments(new THREE.EdgesGeometry(hull),wireMat))
  const dots = instance(new THREE.SphereGeometry(1,8,8),vertices.length,WHITE,.95)
  vertices.forEach((v,i)=>{put(dots,i,v,.085);dots.setColorAt(i,new THREE.Color(i<4?ORANGE:WHITE))})
  group.add(dots)
  const path=[0,6,7,8,3,4].map(i=>vertices[i].clone().multiplyScalar(1.02))
  group.add(line(path,ORANGE,.9))
  const traveler = new THREE.Mesh(new THREE.SphereGeometry(.15,16,12),material(ORANGE,1))
  group.add(traveler)
  return {group,path,traveler}
}

function makeTree() {
  const group = new THREE.Group()
  const points: THREE.Vector3[] = []
  const links: [number,number][] = []
  for(let depth=0;depth<4;depth++){
    const count=2**depth
    for(let j=0;j<count;j++){
      points.push(point(-1.55+3.1*(j+.5)/count,1.5-depth*.95,depth*.12))
      if(depth>0) links.push([2**(depth-1)-1+Math.floor(j/2),2**depth-1+j])
    }
  }
  const nodes=instance(new THREE.SphereGeometry(1,8,8),points.length,BLUE,.9)
  points.forEach((p,i)=>put(nodes,i,p,i===0?.14:.1))
  group.add(nodes)
  const branches: THREE.Line[]=[]
  links.forEach(([a,b])=>{
    const isPath=[1,3,8].includes(b)
    const branch=line([points[a],points[b]],isPath?ORANGE:BLUE,isPath?.9:.65)
    branches.push(branch)
    group.add(branch)
  })
  return {group,nodes,branches}
}

function labelTexture(text: string) {
  const canvas=document.createElement('canvas')
  canvas.width=256;canvas.height=96
  const ctx=canvas.getContext('2d')!
  ctx.clearRect(0,0,256,96)
  ctx.fillStyle='#1b2a41'
  ctx.font='bold 26px Arial'
  ctx.textAlign='center'
  ctx.fillText(text,128,57)
  const texture=new THREE.CanvasTexture(canvas)
  texture.colorSpace=THREE.SRGBColorSpace
  return texture
}
function makePipeline() {
  const group=new THREE.Group()
  const names=['MODEL','PRESOLVE','SOLVE','VALIDATE','SOLUTION']
  const positions=names.map((_,i)=>point((i-2)*1.22,0,(i-2)*.45))
  names.forEach((name,i)=>{
    const panel=new THREE.Mesh(new THREE.BoxGeometry(1.02,1.55,.045),material(WHITE,.32))
    panel.position.copy(positions[i]);panel.rotation.y=-.24
    group.add(panel)
    const edgeMat=new THREE.LineBasicMaterial({color:i===2?ORANGE:BLUE,transparent:true,opacity:.55})
    edgeMat.userData.baseOpacity=.55
    const edge=new THREE.LineSegments(new THREE.EdgesGeometry(panel.geometry),edgeMat)
    edge.position.copy(panel.position);edge.rotation.copy(panel.rotation)
    group.add(edge)
    const textMat=new THREE.MeshBasicMaterial({map:labelTexture(name),transparent:true,opacity:.9,depthWrite:false})
    textMat.userData.baseOpacity=.9
    const face=new THREE.Mesh(new THREE.PlaneGeometry(.86,.32),textMat)
    face.position.copy(positions[i]).add(point(0,0,.07));face.rotation.y=-.24
    group.add(face)
    if(i<4)group.add(line([positions[i].clone().add(point(.5,0,.1)),positions[i+1].clone().add(point(-.5,0,.1))],ORANGE,.65))
  })
  const particles=instance(new THREE.SphereGeometry(1,6,6),26,ORANGE,.9)
  group.add(particles)
  return {group,particles}
}

function makeAlgorithms() {
  const group=new THREE.Group()
  const cloud=instance(new THREE.SphereGeometry(1,6,6),150,BLUE,.65)
  const seed=Array.from({length:150},(_,i)=>{
    const a=i*2.399963
    const r=Math.sqrt((i+.5)/150)*1.9
    return point(Math.cos(a)*r,Math.sin(a)*r,Math.sin(i*7.1)*.7)
  })
  group.add(cloud)
  const bowlLines: THREE.Line[]=[]
  for(let k=-5;k<=5;k++){
    const coords:THREE.Vector3[]=[]
    for(let j=-24;j<=24;j++){
      const x=j/12,y=k/3
      coords.push(point(x,-.7+.15*(x*x+y*y),y*.42))
    }
    const l=line(coords,BLUE,.34);group.add(l);bowlLines.push(l)
  }
  const ball=new THREE.Mesh(new THREE.SphereGeometry(.14,12,10),material(ORANGE,1))
  group.add(ball)
  return {group,cloud,seed,ball,bowlLines}
}

function makeGpu() {
  const group=new THREE.Group()
  const cols=16,rows=10
  const cubes=instance(new THREE.BoxGeometry(.14,.14,.14),cols*rows,BLUE,.83)
  for(let row=0;row<rows;row++)for(let col=0;col<cols;col++){
    const i=row*cols+col
    put(cubes,i,point((col-(cols-1)/2)*.27,((rows-1)/2-row)*.27,Math.sin(col*.55+row*.3)*.25),1)
  }
  group.add(cubes)
  return {group,cubes,cols,rows}
}

function makeConvergence() {
  const group=new THREE.Group()
  const count=130
  const nodes=instance(new THREE.SphereGeometry(1,6,6),count,BLUE,.8)
  const seed=Array.from({length:count},(_,i)=>{
    const a=i*2.399963,r=Math.sqrt((i+.5)/count)*2.2
    return point(Math.cos(a)*r,Math.sin(a)*r,Math.sin(i*.49)*.7)
  })
  const center=new THREE.Mesh(new THREE.SphereGeometry(.21,20,16),material(ORANGE,1))
  group.add(nodes,center)
  return {group,nodes,seed,center}
}

/** One fixed canvas carries every explanatory 3D state. The 2D network remains the mobile and reduced-motion fallback. */
export default function SpatialJourney({progress}:{progress:number}) {
  const canvasRef=useRef<HTMLCanvasElement>(null)
  const progressRef=useRef(progress)
  progressRef.current=progress
  const [desktop,setDesktop]=useState(()=>window.matchMedia('(min-width: 801px) and (prefers-reduced-motion: no-preference)').matches)
  const [use3d,setUse3d]=useState(false)

  useEffect(()=>{
    const media=window.matchMedia('(min-width: 801px) and (prefers-reduced-motion: no-preference)')
    const update=()=>{setUse3d(false);setDesktop(media.matches)}
    media.addEventListener('change',update)
    return ()=>media.removeEventListener('change',update)
  },[])

  useEffect(()=>{
    const canvas=canvasRef.current
    if(!canvas||!desktop)return
    const context=canvas.getContext('webgl2') ?? canvas.getContext('webgl')
    if(!context)return
    let renderer:THREE.WebGLRenderer
    try { renderer=new THREE.WebGLRenderer({canvas,context,alpha:true,antialias:true,powerPreference:'low-power'}) }
    catch { return }
    renderer.setPixelRatio(Math.min(window.devicePixelRatio||1,1.5))
    renderer.setClearColor(0xffffff,0)
    const scene=new THREE.Scene()
    const camera=new THREE.PerspectiveCamera(40,window.innerWidth/window.innerHeight,.1,100)
    camera.position.set(0,0,12)
    const root=new THREE.Group()
    root.position.x=window.innerWidth>1200?3.25:2.7
    scene.add(root)
    const poly=makePolytope(),tree=makeTree(),pipe=makePipeline(),math=makeAlgorithms(),gpu=makeGpu(),end=makeConvergence()
    const groups=[poly.group,tree.group,pipe.group,math.group,gpu.group,end.group]
    groups.forEach(g=>root.add(g))
    const pointer={x:0,y:0}
    let renderActive=true
    let journeyVisible=true
    let last=0
    let visualProgress=progressRef.current
    gsap.registerPlugin(ScrollTrigger)
    const triggers=stages.map((stage,i)=>ScrollTrigger.create({
      trigger:'#'+stage.id,start:'top 55%',end:'bottom 55%',
      onUpdate:self=>{visualProgress=i+self.progress}
    }))
    const lenis=new Lenis({autoRaf:false,anchors:true,duration:1.05})
    const tick=(time:number)=>lenis.raf(time*1000)
    gsap.ticker.add(tick)
    lenis.on('scroll',ScrollTrigger.update)
    gsap.ticker.lagSmoothing(0)

    const resize=()=>{
      const w=window.innerWidth,h=window.innerHeight
      renderer.setSize(w,h,false)
      camera.aspect=w/h;camera.updateProjectionMatrix()
      root.position.x=w>1200?3.25:2.7
      ScrollTrigger.refresh()
    }
    const mouse=(e:MouseEvent)=>{pointer.x=(e.clientX/window.innerWidth-.5)*.25;pointer.y=(e.clientY/window.innerHeight-.5)*.15}
    const observe=new IntersectionObserver(entries=>{journeyVisible=entries[0]?.isIntersecting??true;renderActive=journeyVisible&&!document.hidden},{threshold:0})
    const main=document.querySelector('main')
    if(main)observe.observe(main)
    const visibility=()=>{renderActive=journeyVisible&&!document.hidden}
    document.addEventListener('visibilitychange',visibility)
    window.addEventListener('resize',resize)
    window.addEventListener('pointermove',mouse,{passive:true})
    resize()
    setUse3d(true)

    const tempColor=new THREE.Color()
    const tempPosition=new THREE.Vector3()
    const render=(time:number)=>{
      if(!renderActive||time-last<33)return
      last=time
      // ScrollTrigger owns the visual position; the app's section observer initializes it.
      if(Math.abs(visualProgress-progressRef.current)>.7)visualProgress=progressRef.current
      const p=clamp(Number.isFinite(visualProgress)?visualProgress:progressRef.current,0,stages.length-1)
      const stage=Math.round(p)
      const weight=(targets:number[])=>Math.max(...targets.map(n=>clamp(1-Math.abs(p-n)/1.12)))
      setOpacity(poly.group,weight([0,1]))
      setOpacity(tree.group,weight([2,6]))
      setOpacity(pipe.group,weight([3,5]))
      setOpacity(math.group,weight([4,7]))
      setOpacity(gpu.group,weight([8,9]))
      setOpacity(end.group,weight([10]))
      root.rotation.y+=(pointer.x-root.rotation.y)*.06
      root.rotation.x+=(pointer.y-root.rotation.x)*.06
      const phase=time*.0004
      if(poly.group.visible){
        poly.group.rotation.y=Math.sin(phase)*.1
        const s=((p*.65+phase*.35)%1)*(poly.path.length-1),i=Math.min(poly.path.length-2,Math.floor(s))
        poly.traveler.position.copy(poly.path[i]).lerp(poly.path[i+1],s-i)
      }
      if(tree.group.visible){
        const eliminated=clamp((p-1.8)/1.2)
        for(let i=0;i<tree.nodes.count;i++){
          const onPath=[0,1,3,8].includes(i)
          tempColor.setHex(onPath?ORANGE:BLUE).lerp(FADED_BLUE,onPath?0:eliminated*.85)
          tree.nodes.setColorAt(i,tempColor)
        }
        if(tree.nodes.instanceColor)tree.nodes.instanceColor.needsUpdate=true
        tree.branches.forEach((b,i)=>{
          const onPath=[0,2,7].includes(i)
          const m=b.material as THREE.LineBasicMaterial
          m.opacity=(onPath?.9:.65*(1-eliminated*.82))*weight([2,6])
        })
      }
      if(pipe.group.visible){
        for(let i=0;i<pipe.particles.count;i++){
          const t=((i/pipe.particles.count+phase*.5)%1)
          put(pipe.particles,i,point(-3+t*6,Math.sin(t*20)*.06,0),.05)
        }
        pipe.particles.instanceMatrix.needsUpdate=true
        pipe.group.rotation.y=Math.sin(phase*.7)*.09
      }
      if(math.group.visible){
        const shrink=stage===4?clamp((p-3.6)/.9):.58
        math.seed.forEach((pos,i)=>put(math.cloud,i,tempPosition.copy(pos).multiplyScalar(1-shrink*.65),i%9===0?.065:.037))
        math.cloud.instanceMatrix.needsUpdate=true
        const t=clamp((p-3.9)/.8)
        const x=1.35*(1-t),y=.65*(1-t)
        math.ball.position.set(x,-.7+.15*(x*x+y*y),y*.42)
      }
      if(gpu.group.visible){
        const warp=Math.floor((time*.003)%gpu.cols)
        for(let i=0;i<gpu.cubes.count;i++){
          const col=i%gpu.cols
          gpu.cubes.setColorAt(i,tempColor.setHex(Math.abs(col-warp)<3?ORANGE:BLUE))
        }
        if(gpu.cubes.instanceColor)gpu.cubes.instanceColor.needsUpdate=true
        gpu.group.rotation.y=Math.sin(phase)*.08
      }
      if(end.group.visible){
        const collapse=clamp((p-9.1)/.9)
        end.seed.forEach((pos,i)=>put(end.nodes,i,tempPosition.copy(pos).multiplyScalar(1-collapse*.94),.035+.04*collapse))
        end.nodes.instanceMatrix.needsUpdate=true
        end.center.scale.setScalar(1+collapse*.75+Math.sin(time*.004)*.07)
      }
      renderer.render(scene,camera)
    }
    renderer.setAnimationLoop(render)
    return ()=>{
      renderer.setAnimationLoop(null)
      triggers.forEach(t=>t.kill())
      gsap.ticker.remove(tick)
      lenis.destroy()
      observe.disconnect()
      document.removeEventListener('visibilitychange',visibility)
      window.removeEventListener('resize',resize)
      window.removeEventListener('pointermove',mouse)
      groups.forEach(g=>g.traverse(obj=>{
        const mesh=obj as THREE.Mesh
        mesh.geometry?.dispose()
        const mat=mesh.material as THREE.Material & {map?:THREE.Texture}
        mat?.map?.dispose()
        mat?.dispose?.()
      }))
      renderer.dispose()
      renderer.forceContextLoss()
    }
  },[desktop])

  return <><canvas className={`spatial-journey ${desktop&&use3d?'ready':''}`} ref={canvasRef} aria-hidden="true"/>{(!desktop||!use3d)&&<NetworkJourney progress={progress}/>}</>
}
