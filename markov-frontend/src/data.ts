export const stages = [
  { id: 'entry', index: '00', label: 'Entry', phase: 'INITIAL STATE', title: 'Optimization for a Higher Yield.' },
  { id: 'problem', index: '01', label: 'The problem', phase: 'SEARCH SPACE', title: 'The search space is vast.' },
  { id: 'idea', index: '02', label: 'The idea', phase: 'TRANSITION', title: 'Move from search to convergence.' },
  { id: 'solver', index: '03', label: 'The solver', phase: 'ENGINE', title: 'A solver built around decisions.' },
  { id: 'algorithms', index: '04', label: 'Algorithms', phase: 'MATHEMATICS', title: 'The mathematics underneath.' },
  { id: 'flow', index: '05', label: 'The flow', phase: 'OPTIMIZATION FLOW', title: 'Every decision has a state.' },
  { id: 'applications', index: '06', label: 'Applications', phase: 'APPLICATIONS', title: 'Optimization is everywhere.' },
  { id: 'principles', index: '07', label: 'Principles', phase: 'DESIGN PRINCIPLES', title: 'Built around the search.' },
  { id: 'gpu', index: '08', label: 'GPU research', phase: 'SPARSE COMPUTE', title: 'Parallelize where the math allows.' },
  { id: 'faq', index: '09', label: 'FAQ', phase: 'COMMON QUESTIONS', title: 'Questions along the way.' },
  { id: 'convergence', index: '10', label: 'Convergence', phase: 'OPTIMAL STATE', title: 'Converge on better decisions.' },
] as const

export type StageId = (typeof stages)[number]['id']
