import { useEffect, useRef } from 'react'

type Props = { progress: number; variant?: 'full' | 'quiet' }
type Point = { x: number; y: number }

const columns = 64
const rows = 19
const routeRows = [4, 5, 4, 6, 5, 7, 6, 5, 4, 5, 4]
const point = (column: number, row: number): Point => ({
  x: column * 108 * (0.58 + row * 0.044) + (row % 2) * 33 + Math.sin(column * 0.73 + row * 1.41) * 10,
  y: row * 78 + Math.sin(column * 0.48 + row * 0.77) * 12,
})
const waypoint = (stage: number) => point(4 + stage * 4, routeRows[stage])
const lerp = (start: number, end: number, amount: number) => start + (end - start) * amount

/** One connected network follows the reader through the whole story. */
export default function NetworkJourney({ progress, variant = 'full' }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const progressRef = useRef(progress)
  progressRef.current = progress

  useEffect(() => {
    const canvas = canvasRef.current
    const context = canvas?.getContext('2d', { alpha: true })
    if (!canvas || !context) return
    const motionPreference = window.matchMedia('(prefers-reduced-motion: reduce)')
    const points = Array.from({ length: rows }, (_, row) =>
      Array.from({ length: columns }, (_, column) => point(column, row)),
    )
    let width = 0
    let height = 0
    let panX = Number.NaN
    let panY = Number.NaN
    let frame = 0
    let lastDraw = 0
    let previousPosition = Number.NaN

    const resize = () => {
      previousPosition = Number.NaN
      width = window.innerWidth
      height = window.innerHeight
      const ratio = Math.min(window.devicePixelRatio || 1, 1.5)
      canvas.width = Math.round(width * ratio)
      canvas.height = Math.round(height * ratio)
      canvas.style.width = `${width}px`
      canvas.style.height = `${height}px`
      context.setTransform(ratio, 0, 0, ratio, 0, 0)
    }

    const draw = (time: number) => {
      frame = requestAnimationFrame(draw)
      if (document.hidden || time - lastDraw < (width < 800 ? 32 : 24)) return
      lastDraw = time
      const position = Math.max(0, Math.min(routeRows.length - 1, progressRef.current))
      const reducedMotion = motionPreference.matches
      const index = Math.min(routeRows.length - 2, Math.floor(position))
      const fraction = position - index
      const before = waypoint(index)
      const after = waypoint(index + 1)
      const focus = { x: lerp(before.x, after.x, fraction), y: lerp(before.y, after.y, fraction) }
      const anchorX = width < 800 ? width * 0.72 : width * 0.76
      const anchorY = height * (width < 800 ? 0.52 : 0.49)
      const targetX = anchorX - focus.x
      const targetY = anchorY - focus.y
      const settled = Math.abs(position - previousPosition) < 0.001 && Math.abs(panX - targetX) < 0.15 && Math.abs(panY - targetY) < 0.15
      if (settled || (reducedMotion && Math.abs(position - previousPosition) < 0.001)) return
      previousPosition = position
      panX = Number.isNaN(panX) || reducedMotion ? targetX : lerp(panX, targetX, 0.18)
      panY = Number.isNaN(panY) || reducedMotion ? targetY : lerp(panY, targetY, 0.18)
      context.clearRect(0, 0, width, height)

      const screen = (p: Point) => ({ x: p.x + panX, y: p.y + panY })
      const fade = (x: number) => Math.max(0.08, Math.min(0.9, (x - width * 0.34) / (width * 0.52)))

      // Neighboring rows share vertices, so the entire field moves as one surface.
      for (let row = 0; row < rows; row++) {
        for (let column = 0; column < columns; column++) {
          const start = screen(points[row][column])
          if (start.x < -180 || start.x > width + 180 || start.y < -180 || start.y > height + 180) continue
          const neighbors = [
            column + 1 < columns ? points[row][column + 1] : null,
            row + 1 < rows ? points[row + 1][column] : null,
            row + 1 < rows && column + 1 < columns ? points[row + 1][column + 1] : null,
          ]
          for (const neighbor of neighbors) {
            if (!neighbor) continue
            const end = screen(neighbor)
            const explored = column < 4 + position * 4 - 1 ? 0.3 : 1
            const strength = fade((start.x + end.x) / 2) * explored
            context.strokeStyle = (start.x + end.x) / 2 > width * 0.58
              ? `rgba(255, 225, 201, ${0.7 * strength})`
              : `rgba(48, 103, 151, ${0.55 * strength})`
            context.lineWidth = 0.85 + row * 0.045
            context.beginPath()
            context.moveTo(start.x, start.y)
            context.lineTo(end.x, end.y)
            context.stroke()
          }
        }
      }

      for (let row = 0; row < rows; row++) {
        for (let column = 0; column < columns; column++) {
          const node = screen(points[row][column])
          if (node.x < -20 || node.x > width + 20 || node.y < -20 || node.y > height + 20) continue
          const explored = column < 4 + position * 4 - 1 ? 0.3 : 1
          const strength = fade(node.x) * explored
          const radius = 2.2 + row * 0.16
          context.fillStyle = `rgba(245, 152, 86, ${0.25 * strength})`
          context.beginPath()
          context.arc(node.x, node.y, radius + 2.5, 0, Math.PI * 2)
          context.fill()
          context.fillStyle = node.x > width * 0.58
            ? `rgba(255, 247, 228, ${0.95 * strength})`
            : `rgba(255, 244, 225, ${0.9 * strength})`
          context.strokeStyle = node.x > width * 0.58
            ? `rgba(255, 229, 202, ${0.85 * strength})`
            : `rgba(75, 120, 159, ${0.75 * strength})`
          context.lineWidth = 0.8
          context.beginPath()
          context.arc(node.x, node.y, radius, 0, Math.PI * 2)
          context.fill()
          context.stroke()
        }
      }

      // The orange route grows continuously while the mesh moves under the focus.
      context.save()
      context.beginPath()
      context.rect(width * (width < 800 ? 0.48 : 0.51), 0, width, height)
      context.clip()
      context.lineCap = 'round'
      context.lineJoin = 'round'
      context.beginPath()
      const routeStart = screen(waypoint(0))
      context.moveTo(routeStart.x, routeStart.y)
      for (let stage = 1; stage <= index; stage++) {
        const node = screen(waypoint(stage))
        context.lineTo(node.x, node.y)
      }
      const active = screen(focus)
      context.lineTo(active.x, active.y)
      context.strokeStyle = 'rgba(244, 129, 22, 0.2)'
      context.lineWidth = 12
      context.stroke()
      context.strokeStyle = '#f48116'
      context.lineWidth = 2.4
      context.stroke()

      for (let stage = 0; stage <= index; stage++) {
        const node = screen(waypoint(stage))
        if (node.x < -12 || node.x > width + 12 || node.y < -12 || node.y > height + 12) continue
        context.beginPath()
        context.arc(node.x, node.y, 3.5, 0, Math.PI * 2)
        context.fillStyle = '#f48116'
        context.fill()
      }

      context.fillStyle = 'rgba(244, 129, 22, 0.13)'
      context.beginPath()
      context.arc(active.x, active.y, 31, 0, Math.PI * 2)
      context.fill()
      context.strokeStyle = 'rgba(244, 129, 22, 0.6)'
      context.lineWidth = 1
      context.beginPath()
      context.arc(active.x, active.y, 19, 0, Math.PI * 2)
      context.stroke()
      context.fillStyle = '#f48116'
      context.beginPath()
      context.arc(active.x, active.y, 7, 0, Math.PI * 2)
      context.fill()
      context.restore()
    }

    resize()
    frame = requestAnimationFrame(draw)
    window.addEventListener('resize', resize)
    return () => {
      cancelAnimationFrame(frame)
      window.removeEventListener('resize', resize)
    }
  }, [])

  return <canvas ref={canvasRef} className={`network-journey ${variant === 'quiet' ? 'network-quiet' : ''}`} aria-hidden="true" />
}
