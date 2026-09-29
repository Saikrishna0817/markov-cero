import { useEffect, useRef } from 'react'

type Props = { progress: number }
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

/** One connected mesh translates as the reader moves between its ten waypoints. */
export default function NetworkJourney({ progress }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const progressRef = useRef(progress)
  progressRef.current = progress

  useEffect(() => {
    const canvas = canvasRef.current
    const context = canvas?.getContext('2d', { alpha: true })
    if (!canvas || !context) return
    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches
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
      if (time - lastDraw < (width < 800 ? 32 : 20)) return
      lastDraw = time
      const position = Math.max(0, Math.min(routeRows.length - 1, progressRef.current))
      if (reducedMotion && Math.abs(position - previousPosition) < 0.001) return
      previousPosition = position
      const index = Math.min(routeRows.length - 2, Math.floor(position))
      const fraction = position - index
      const before = waypoint(index)
      const after = waypoint(index + 1)
      const focus = { x: lerp(before.x, after.x, fraction), y: lerp(before.y, after.y, fraction) }
      const anchorX = width < 800 ? 35 : Math.max(90, width * 0.07)
      const anchorY = width < 800 ? 104 : 154
      const targetX = anchorX - focus.x
      const targetY = anchorY - focus.y
      panX = Number.isNaN(panX) || reducedMotion ? targetX : lerp(panX, targetX, 0.12)
      panY = Number.isNaN(panY) || reducedMotion ? targetY : lerp(panY, targetY, 0.12)
      context.clearRect(0, 0, width, height)

      const screen = (p: Point) => ({ x: p.x + panX, y: p.y + panY })
      const fade = (x: number) => Math.max(0.09, Math.min(0.75, (x - anchorX + 110) / 650))

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
            const explored = column < 4 + position * 4 - 1 ? 0.28 : 1
            const strength = fade((start.x + end.x) / 2) * explored
            context.strokeStyle = (start.x + end.x) / 2 > width * 0.58
              ? `rgba(255, 225, 201, ${0.65 * strength})`
              : `rgba(48, 103, 151, ${0.49 * strength})`
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
          const explored = column < 4 + position * 4 - 1 ? 0.28 : 1
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

      const active = screen(waypoint(Math.round(position)))
      context.fillStyle = 'rgba(244, 129, 22, 0.12)'
      context.beginPath()
      context.arc(active.x, active.y, 23, 0, Math.PI * 2)
      context.fill()
      context.fillStyle = '#f48116'
      context.beginPath()
      context.arc(active.x, active.y, 8, 0, Math.PI * 2)
      context.fill()
    }

    resize()
    frame = requestAnimationFrame(draw)
    window.addEventListener('resize', resize)
    return () => {
      cancelAnimationFrame(frame)
      window.removeEventListener('resize', resize)
    }
  }, [])

  return <canvas ref={canvasRef} className="network-journey" aria-hidden="true" />
}
