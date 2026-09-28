"""Augment existing callgraph evidence with audited SysTick -> priority-5 IRQ."""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'stage5mr5c'))
from analyze_ram_stack import parse_callgraphs


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--analysis', type=Path, required=True)
    p.add_argument('--callgraph-root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    old = json.loads((a.analysis / 'stack_watermark.json').read_text())
    ram = json.loads((a.analysis / 'ram_layout.json').read_text())
    reports, _, _, _, _ = parse_callgraphs(a.callgraph_root)
    systick = next(r for r in reports if r['function'] == 'SysTick_Handler')
    extra = systick['static_chain_bytes'] + 32 + 8
    conservative = old['bounded_conservative_stack_bytes'] + extra
    margin = ram['total_static_to_estack_bytes'] - conservative
    result = dict(classification='STATIC_NOT_RUNTIME_WATERMARK',
        legacy_one_irq_bound_bytes=old['bounded_conservative_stack_bytes'],
        extra_nested_systick_bytes=extra, systick_chain=systick,
        extra_exception_alignment_padding_bytes=8,
        complete_conservative_stack_bytes=conservative,
        collision_margin_bytes=margin, required_margin_bytes=512,
        static_ram_bytes=ram['linker_ram_used_bytes'],
        result='PASS' if margin >= 512 else 'FAIL',
        irq_priority_basis='UART/DMA/TIM4 priority5; SysTick priority15; no other enabled nonfatal priorities')
    a.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    if margin < 512:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
