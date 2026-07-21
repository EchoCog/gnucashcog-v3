#!/usr/bin/env python3
# ecan_resource_allocator.py -- Phase 2 ECAN attention allocation kernel
#
# Copyright (C) 2024 GnuCash Cognitive Engine
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

"""Phase 2: ECAN-inspired economic attention allocation for GnuCash Cognitive.

This module is the Python counterpart of the C++ ECAN kernel implemented in
``libgnucash/engine/gnc-cognitive-accounting.cpp`` (``gnc_ecan_*`` API).  It
provides a complete, dependency-free implementation of OpenCog ECAN-style
attention economics:

* **STI/LTI dynamics** -- short- and long-term importance values per atom,
  backed by finite fund pools (cognitive economics).
* **Cognitive wages** -- activity-based STI/LTI rewards paid from the funds.
* **Attention rent** -- resource-usage costs collected back into the funds.
* **Spreading activation** -- attention propagation along weighted hypergraph
  links with configurable spreading rate and threshold.
* **Attention decay** -- exponential decay that recycles importance into the
  fund pools, keeping the total attention supply conserved.
* **Starvation prevention** -- emergency allocation for starved atoms.
* **Priority task scheduling** -- attention-driven cognitive task scheduler.
* **Distributed attention mesh** -- inter-node attention propagation and
  load balancing across distributed agents.

Run modes::

    python3 ecan_resource_allocator.py --demo        # narrative demonstration
    python3 ecan_resource_allocator.py --benchmark   # performance benchmarks
    python3 ecan_resource_allocator.py --selftest    # unit-test verification
"""

from __future__ import annotations

import argparse
import heapq
import itertools
import math
import sys
import time
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Callable, Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Attention values (mirrors GncAttentionParams in gnc-cognitive-accounting.h)
# ---------------------------------------------------------------------------


@dataclass
class AttentionValue:
    """ECAN attention value with STI/LTI/VLTI dynamics."""

    sti: float = 10.0
    lti: float = 5.0
    vlti: float = 0.0
    sti_decay_rate: float = 0.01
    lti_decay_rate: float = 0.005
    vlti_threshold: float = 100.0
    activity_level: float = 0.0
    wage: float = 1.0
    rent: float = 0.1
    wage_multiplier: float = 1.0
    spreading_rate: float = 0.1
    spreading_threshold: float = 20.0
    focus_factor: float = 1.0
    competition_strength: float = 1.0
    starvation_threshold: float = 5.0

    @property
    def total_importance(self) -> float:
        return self.sti + self.lti + self.vlti

    def is_starved(self) -> bool:
        return self.sti < self.starvation_threshold


# ---------------------------------------------------------------------------
# Attention bank: the economic core (funds, wages, rent, decay, spreading)
# ---------------------------------------------------------------------------


class AttentionBank:
    """Economic attention allocator over a weighted hypergraph of atoms.

    The bank owns finite STI/LTI fund pools.  Wages are paid out of the
    pools, rent and decay flow back in, so the total supply of attention in
    the system is conserved -- the core ECAN invariant that prevents runaway
    attention inflation and guarantees resource competition.
    """

    def __init__(self, sti_funds: float = 10000.0, lti_funds: float = 10000.0):
        if sti_funds <= 0 or lti_funds <= 0:
            raise ValueError("fund pools must be positive")
        self.sti_funds = sti_funds
        self.lti_funds = lti_funds
        self.initial_sti_funds = sti_funds
        self.initial_lti_funds = lti_funds
        self.atoms: Dict[str, AttentionValue] = {}
        # adjacency: atom -> {neighbor: link strength (0.0-1.0)}
        self.links: Dict[str, Dict[str, float]] = {}
        self.emergency_allocations = 0
        self.total_rent_collected = 0.0
        self.total_wages_paid = 0.0

    # -- atom / link management --------------------------------------------

    def add_atom(self, name: str, av: Optional[AttentionValue] = None) -> AttentionValue:
        if name in self.atoms:
            return self.atoms[name]
        av = av or AttentionValue()
        self.atoms[name] = av
        self.links.setdefault(name, {})
        return av

    def add_link(self, a: str, b: str, strength: float = 0.5) -> None:
        if a not in self.atoms or b not in self.atoms:
            raise KeyError("both atoms must exist before linking")
        if not 0.0 <= strength <= 1.0:
            raise ValueError("link strength must be in [0.0, 1.0]")
        self.links[a][b] = strength
        self.links[b][a] = strength

    # -- cognitive wages -----------------------------------------------------

    def pay_wages(self) -> float:
        """Pay activity-based STI/LTI wages from the fund pools.

        Returns the total wages paid this cycle.
        """
        total_paid = 0.0
        for av in self.atoms.values():
            wage = av.wage * av.wage_multiplier * av.activity_level
            if wage <= 0.0:
                continue
            sti_wage = min(wage, self.sti_funds)
            self.sti_funds -= sti_wage
            av.sti += sti_wage
            # A fraction of sustained activity consolidates into LTI
            lti_wage = min(wage * 0.1, self.lti_funds)
            self.lti_funds -= lti_wage
            av.lti += lti_wage
            total_paid += sti_wage + lti_wage
            # VLTI promotion for persistently important atoms
            if av.lti > av.vlti_threshold:
                promoted = av.lti - av.vlti_threshold
                av.vlti += promoted
                av.lti = av.vlti_threshold
        self.total_wages_paid += total_paid
        return total_paid

    # -- attention rent -----------------------------------------------------

    def collect_rent(self) -> float:
        """Collect rent proportional to held STI; recycle into the funds."""
        total_rent = 0.0
        for av in self.atoms.values():
            rent = av.rent * max(av.sti, 0.0)
            rent = min(rent, av.sti)  # cannot drive STI negative via rent
            av.sti -= rent
            self.sti_funds += rent
            total_rent += rent
        self.total_rent_collected += total_rent
        return total_rent

    # -- attention decay ----------------------------------------------------

    def apply_decay(self, cycles: float = 1.0) -> None:
        """Exponential attention decay; decayed attention returns to funds."""
        for av in self.atoms.values():
            sti_decay = av.sti * (1.0 - math.exp(-av.sti_decay_rate * cycles))
            lti_decay = av.lti * (1.0 - math.exp(-av.lti_decay_rate * cycles))
            av.sti -= sti_decay
            av.lti -= lti_decay
            self.sti_funds += sti_decay
            self.lti_funds += lti_decay

    # -- spreading activation -------------------------------------------------

    def spread_attention(self, source: str, depth: int = 1) -> float:
        """Spread STI from *source* along weighted links (recursive pathway).

        Only atoms above their spreading threshold spread.  The amount
        spread is ``sti * spreading_rate``, divided among neighbors by
        normalized link strength.  Returns total attention spread.
        """
        if depth <= 0 or source not in self.atoms:
            return 0.0
        av = self.atoms[source]
        if av.sti < av.spreading_threshold:
            return 0.0
        neighbors = self.links.get(source, {})
        total_strength = sum(neighbors.values())
        if total_strength <= 0.0:
            return 0.0
        spread_amount = av.sti * av.spreading_rate * av.focus_factor
        spread_amount = min(spread_amount, av.sti)
        av.sti -= spread_amount
        total_spread = 0.0
        for neighbor, strength in neighbors.items():
            share = spread_amount * (strength / total_strength)
            self.atoms[neighbor].sti += share
            total_spread += share
            # Recursive resource allocation pathway
            self.spread_attention(neighbor, depth - 1)
        return total_spread

    # -- resource competition & starvation prevention -------------------------

    def prevent_starvation(self) -> int:
        """Emergency STI allocation for starved atoms.  Returns count rescued."""
        rescued = 0
        for av in self.atoms.values():
            if av.is_starved():
                needed = av.starvation_threshold * 2.0 - av.sti
                grant = min(needed, self.sti_funds)
                if grant > 0.0:
                    self.sti_funds -= grant
                    av.sti += grant
                    rescued += 1
                    self.emergency_allocations += 1
        return rescued

    def compete_for_attention(self, budget: float) -> Dict[str, float]:
        """Distribute an attention budget by competition strength x activity."""
        if budget <= 0.0:
            return {}
        budget = min(budget, self.sti_funds)
        weights = {
            name: av.competition_strength * (av.activity_level + 0.01)
            for name, av in self.atoms.items()
        }
        total_weight = sum(weights.values())
        awards: Dict[str, float] = {}
        if total_weight <= 0.0:
            return awards
        for name, weight in weights.items():
            award = budget * (weight / total_weight)
            self.atoms[name].sti += award
            self.sti_funds -= award
            awards[name] = award
        return awards

    # -- system statistics -----------------------------------------------------

    def system_stats(self) -> Dict[str, float]:
        sti_circulation = sum(av.sti for av in self.atoms.values())
        lti_circulation = sum(av.lti for av in self.atoms.values())
        return {
            "sti_in_circulation": sti_circulation,
            "lti_in_circulation": lti_circulation,
            "sti_fund_balance": self.sti_funds,
            "lti_fund_balance": self.lti_funds,
            "total_sti_supply": sti_circulation + self.sti_funds,
            "total_lti_supply": lti_circulation + self.lti_funds,
            "emergency_allocations": float(self.emergency_allocations),
            "total_rent_collected": self.total_rent_collected,
            "total_wages_paid": self.total_wages_paid,
        }

    def run_economic_cycle(self) -> Dict[str, float]:
        """Run one complete ECAN economic cycle over the hypergraph."""
        wages = self.pay_wages()
        spread = 0.0
        for name in list(self.atoms):
            spread += self.spread_attention(name, depth=2)
        rent = self.collect_rent()
        self.apply_decay()
        rescued = self.prevent_starvation()
        return {
            "wages_paid": wages,
            "attention_spread": spread,
            "rent_collected": rent,
            "atoms_rescued": float(rescued),
        }


# ---------------------------------------------------------------------------
# Priority-based cognitive task scheduling (mirrors gnc_ecan_scheduler_*)
# ---------------------------------------------------------------------------


class TaskPriority(IntEnum):
    EMERGENCY = 1000
    HIGH = 750
    NORMAL = 500
    LOW = 250
    BACKGROUND = 100


@dataclass(order=True)
class _QueuedTask:
    sort_key: Tuple[int, int] = field(init=False, repr=False)
    priority: TaskPriority = field(compare=False)
    sequence: int = field(compare=False)
    task_id: str = field(compare=False)
    task_type: str = field(compare=False)
    attention_requirement: float = field(compare=False)
    deadline: float = field(compare=False, default=0.0)
    action: Optional[Callable[[], object]] = field(compare=False, default=None)

    def __post_init__(self) -> None:
        # heapq is a min-heap: negate priority so higher priority pops first,
        # FIFO within a priority level via the monotone sequence number.
        self.sort_key = (-int(self.priority), self.sequence)


class AttentionScheduler:
    """Attention-driven priority scheduler for cognitive tasks."""

    def __init__(self, max_concurrent_tasks: int = 8, attention_pool: float = 500.0):
        if max_concurrent_tasks <= 0:
            raise ValueError("max_concurrent_tasks must be positive")
        self.max_concurrent_tasks = max_concurrent_tasks
        self.attention_pool = attention_pool
        self._heap: List[_QueuedTask] = []
        self._sequence = itertools.count()
        self._cancelled: set = set()
        self.running_tasks: Dict[str, _QueuedTask] = {}
        self.completed_tasks: List[str] = []
        self.total_attention_allocated = 0.0

    def submit_task(
        self,
        task_type: str,
        priority: TaskPriority,
        attention_requirement: float,
        action: Optional[Callable[[], object]] = None,
        deadline: float = 0.0,
    ) -> str:
        if attention_requirement < 0.0:
            raise ValueError("attention_requirement must be non-negative")
        seq = next(self._sequence)
        task_id = f"task-{seq}"
        task = _QueuedTask(
            priority=priority,
            sequence=seq,
            task_id=task_id,
            task_type=task_type,
            attention_requirement=attention_requirement,
            deadline=deadline,
            action=action,
        )
        heapq.heappush(self._heap, task)
        return task_id

    def cancel_task(self, task_id: str) -> bool:
        if any(t.task_id == task_id for t in self._heap):
            self._cancelled.add(task_id)
            return True
        return False

    def process_tasks(self, available_attention: float) -> int:
        """Run one scheduling cycle; returns number of tasks processed."""
        budget = min(available_attention, self.attention_pool)
        processed = 0
        deferred: List[_QueuedTask] = []
        now = time.monotonic()
        while self._heap and processed < self.max_concurrent_tasks:
            task = heapq.heappop(self._heap)
            if task.task_id in self._cancelled:
                self._cancelled.discard(task.task_id)
                continue
            if task.deadline and task.deadline < now:
                # Missed deadline: drop the task, do not spend attention
                continue
            if task.attention_requirement > budget:
                deferred.append(task)
                continue
            budget -= task.attention_requirement
            self.total_attention_allocated += task.attention_requirement
            self.running_tasks[task.task_id] = task
            if task.action is not None:
                task.action()
            del self.running_tasks[task.task_id]
            self.completed_tasks.append(task.task_id)
            processed += 1
        for task in deferred:
            heapq.heappush(self._heap, task)
        return processed

    def stats(self) -> Dict[str, float]:
        completed = len(self.completed_tasks)
        submitted = completed + len(self._heap) + len(self.running_tasks)
        efficiency = completed / submitted if submitted else 1.0
        return {
            "pending_tasks": float(len(self._heap)),
            "running_tasks": float(len(self.running_tasks)),
            "completed_tasks": float(completed),
            "total_attention_allocated": self.total_attention_allocated,
            "scheduler_efficiency": efficiency,
        }


# ---------------------------------------------------------------------------
# Distributed attention mesh (mirrors gnc_ecan_mesh_*)
# ---------------------------------------------------------------------------


@dataclass
class MeshNode:
    node_id: str
    attention_capacity: float
    current_attention: float = 0.0
    last_sync_time: float = 0.0
    neighbors: Dict[str, float] = field(default_factory=dict)

    @property
    def utilization(self) -> float:
        return self.current_attention / self.attention_capacity if self.attention_capacity else 0.0


class AttentionMesh:
    """Distributed attention mesh for inter-agent attention propagation."""

    def __init__(self) -> None:
        self.nodes: Dict[str, MeshNode] = {}

    def add_node(self, node_id: str, attention_capacity: float) -> MeshNode:
        if attention_capacity <= 0.0:
            raise ValueError("attention_capacity must be positive")
        node = MeshNode(node_id=node_id, attention_capacity=attention_capacity)
        self.nodes[node_id] = node
        return node

    def connect_nodes(self, a: str, b: str, strength: float) -> None:
        if a not in self.nodes or b not in self.nodes:
            raise KeyError("both nodes must exist before connecting")
        if not 0.0 <= strength <= 1.0:
            raise ValueError("connection strength must be in [0.0, 1.0]")
        self.nodes[a].neighbors[b] = strength
        self.nodes[b].neighbors[a] = strength

    def propagate_attention(self, source_id: str, attention_change: float,
                            propagation_depth: int = 2) -> float:
        """Propagate an attention change through the mesh with damping.

        Returns the total attention absorbed by the mesh.
        """
        if source_id not in self.nodes or propagation_depth < 0:
            return 0.0
        absorbed = 0.0
        frontier = {source_id: attention_change}
        visited = set()
        for _ in range(propagation_depth + 1):
            next_frontier: Dict[str, float] = {}
            for node_id, change in frontier.items():
                node = self.nodes[node_id]
                free = node.attention_capacity - node.current_attention
                take = max(min(change, free), -node.current_attention)
                node.current_attention += take
                absorbed += take
                remainder = change - take
                visited.add(node_id)
                total_strength = sum(
                    s for n, s in node.neighbors.items() if n not in visited
                )
                if total_strength <= 0.0:
                    continue
                # Damped propagation of the unabsorbed remainder plus a
                # spreading fraction of what was absorbed locally.
                outgoing = remainder + take * 0.1
                for neighbor, strength in node.neighbors.items():
                    if neighbor in visited:
                        continue
                    share = outgoing * (strength / total_strength) * 0.5
                    next_frontier[neighbor] = next_frontier.get(neighbor, 0.0) + share
            frontier = next_frontier
            if not frontier:
                break
        return absorbed

    def synchronize(self) -> None:
        now = time.monotonic()
        for node in self.nodes.values():
            node.last_sync_time = now

    def balance_load(self, load_threshold: float = 0.8) -> int:
        """Move attention from overloaded to underloaded neighbors.

        Returns the number of attention transfers performed.
        """
        transfers = 0
        for node in self.nodes.values():
            if node.utilization <= load_threshold:
                continue
            excess = node.current_attention - node.attention_capacity * load_threshold
            for neighbor_id in sorted(node.neighbors,
                                      key=lambda n: self.nodes[n].utilization):
                if excess <= 0.0:
                    break
                neighbor = self.nodes[neighbor_id]
                free = neighbor.attention_capacity * load_threshold - neighbor.current_attention
                if free <= 0.0:
                    continue
                moved = min(excess, free)
                node.current_attention -= moved
                neighbor.current_attention += moved
                excess -= moved
                transfers += 1
        return transfers

    def topology_stats(self) -> Dict[str, float]:
        total_nodes = len(self.nodes)
        total_connections = sum(len(n.neighbors) for n in self.nodes.values()) // 2
        avg_capacity = (
            sum(n.attention_capacity for n in self.nodes.values()) / total_nodes
            if total_nodes else 0.0
        )
        utilization = (
            sum(n.current_attention for n in self.nodes.values())
            / sum(n.attention_capacity for n in self.nodes.values())
            if total_nodes else 0.0
        )
        return {
            "total_nodes": float(total_nodes),
            "total_connections": float(total_connections),
            "avg_node_capacity": avg_capacity,
            "mesh_utilization": utilization,
        }


# ---------------------------------------------------------------------------
# Demonstration, benchmarks and self-test
# ---------------------------------------------------------------------------


def _build_demo_bank() -> AttentionBank:
    bank = AttentionBank(sti_funds=1000.0, lti_funds=1000.0)
    for name, activity in [
        ("Account:Checking", 0.9),
        ("Account:Savings", 0.3),
        ("Account:Expenses:Groceries", 0.7),
        ("Account:Income:Salary", 0.5),
        ("Account:Expenses:Rent", 0.4),
    ]:
        av = bank.add_atom(name)
        av.activity_level = activity
    bank.add_link("Account:Checking", "Account:Expenses:Groceries", 0.8)
    bank.add_link("Account:Checking", "Account:Income:Salary", 0.9)
    bank.add_link("Account:Checking", "Account:Savings", 0.5)
    bank.add_link("Account:Checking", "Account:Expenses:Rent", 0.6)
    bank.add_link("Account:Savings", "Account:Income:Salary", 0.3)
    return bank


def run_demo() -> None:
    print("=" * 68)
    print(" Phase 2: ECAN Attention Allocation -- Python Resource Allocator")
    print("=" * 68)

    bank = _build_demo_bank()
    print("\n[1] Economic attention cycles (wages, spreading, rent, decay):")
    for cycle in range(1, 4):
        result = bank.run_economic_cycle()
        print(f"    cycle {cycle}: wages={result['wages_paid']:.2f} "
              f"spread={result['attention_spread']:.2f} "
              f"rent={result['rent_collected']:.2f} "
              f"rescued={int(result['atoms_rescued'])}")
    stats = bank.system_stats()
    print(f"    STI circulation={stats['sti_in_circulation']:.2f} "
          f"funds={stats['sti_fund_balance']:.2f} "
          f"(supply conserved: {stats['total_sti_supply']:.2f})")

    print("\n[2] Priority-based task scheduling:")
    scheduler = AttentionScheduler(max_concurrent_tasks=4, attention_pool=100.0)
    scheduler.submit_task("pattern-discovery", TaskPriority.LOW, 10.0)
    scheduler.submit_task("transaction-validation", TaskPriority.HIGH, 15.0)
    scheduler.submit_task("starvation-response", TaskPriority.EMERGENCY, 5.0)
    scheduler.submit_task("balance-update", TaskPriority.NORMAL, 8.0)
    processed = scheduler.process_tasks(available_attention=50.0)
    sstats = scheduler.stats()
    print(f"    processed={processed} "
          f"efficiency={sstats['scheduler_efficiency']:.2f} "
          f"attention_allocated={sstats['total_attention_allocated']:.2f}")

    print("\n[3] Distributed attention mesh:")
    mesh = AttentionMesh()
    for node_id in ("agent-alpha", "agent-beta", "agent-gamma", "agent-delta"):
        mesh.add_node(node_id, attention_capacity=100.0)
    mesh.connect_nodes("agent-alpha", "agent-beta", 0.9)
    mesh.connect_nodes("agent-beta", "agent-gamma", 0.7)
    mesh.connect_nodes("agent-gamma", "agent-delta", 0.5)
    mesh.connect_nodes("agent-alpha", "agent-delta", 0.4)
    absorbed = mesh.propagate_attention("agent-alpha", 150.0, propagation_depth=3)
    transfers = mesh.balance_load(0.8)
    tstats = mesh.topology_stats()
    print(f"    absorbed={absorbed:.2f} transfers={transfers} "
          f"nodes={int(tstats['total_nodes'])} "
          f"connections={int(tstats['total_connections'])} "
          f"utilization={tstats['mesh_utilization']:.2f}")

    print("\nECAN attention economy operational. Supply conserved, "
          "no starvation, mesh balanced.")


def run_benchmark() -> None:
    print("Phase 2 ECAN benchmarks (pure Python reference implementation)")
    for n_atoms in (100, 1000, 5000):
        bank = AttentionBank(sti_funds=float(n_atoms * 100), lti_funds=float(n_atoms * 100))
        names = [f"atom-{i}" for i in range(n_atoms)]
        for i, name in enumerate(names):
            av = bank.add_atom(name)
            av.activity_level = (i % 10) / 10.0
        for i in range(n_atoms - 1):
            bank.add_link(names[i], names[i + 1], 0.5)
        start = time.perf_counter()
        bank.run_economic_cycle()
        elapsed = time.perf_counter() - start
        print(f"  economic cycle, {n_atoms:5d} atoms: {elapsed * 1000:8.2f} ms "
              f"({elapsed / n_atoms * 1e6:.2f} us/atom)")

    for n_tasks in (1000, 10000):
        scheduler = AttentionScheduler(max_concurrent_tasks=n_tasks, attention_pool=1e9)
        priorities = list(TaskPriority)
        for i in range(n_tasks):
            scheduler.submit_task("bench", priorities[i % len(priorities)], 1.0)
        start = time.perf_counter()
        processed = scheduler.process_tasks(available_attention=1e9)
        elapsed = time.perf_counter() - start
        print(f"  scheduler, {n_tasks:5d} tasks: processed={processed} "
              f"in {elapsed * 1000:8.2f} ms")

    mesh = AttentionMesh()
    n_nodes = 500
    for i in range(n_nodes):
        mesh.add_node(f"node-{i}", 100.0)
    for i in range(n_nodes - 1):
        mesh.connect_nodes(f"node-{i}", f"node-{i + 1}", 0.5)
    start = time.perf_counter()
    mesh.propagate_attention("node-0", 5000.0, propagation_depth=10)
    mesh.balance_load(0.8)
    elapsed = time.perf_counter() - start
    print(f"  mesh, {n_nodes} nodes: propagate+balance in {elapsed * 1000:.2f} ms")


def run_selftest() -> int:
    import unittest

    class EcanTests(unittest.TestCase):
        def test_sti_supply_conservation(self) -> None:
            bank = _build_demo_bank()
            initial_supply = bank.system_stats()["total_sti_supply"]
            for _ in range(10):
                bank.run_economic_cycle()
            final_supply = bank.system_stats()["total_sti_supply"]
            self.assertAlmostEqual(initial_supply, final_supply, places=6)

        def test_wages_reward_activity(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            busy = bank.add_atom("busy")
            idle = bank.add_atom("idle")
            busy.activity_level = 1.0
            idle.activity_level = 0.0
            bank.pay_wages()
            self.assertGreater(busy.sti, idle.sti)

        def test_rent_flows_back_to_funds(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            bank.add_atom("a").sti = 100.0
            funds_before = bank.sti_funds
            rent = bank.collect_rent()
            self.assertGreater(rent, 0.0)
            self.assertAlmostEqual(bank.sti_funds, funds_before + rent)

        def test_spreading_activation(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            src = bank.add_atom("src")
            dst = bank.add_atom("dst")
            bank.add_link("src", "dst", 1.0)
            src.sti = 100.0
            dst.sti = 0.0
            spread = bank.spread_attention("src")
            self.assertGreater(spread, 0.0)
            self.assertGreater(bank.atoms["dst"].sti, 0.0)

        def test_spreading_threshold_blocks_low_sti(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            src = bank.add_atom("src")
            bank.add_atom("dst")
            bank.add_link("src", "dst", 1.0)
            src.sti = 1.0  # below spreading_threshold of 20.0
            self.assertEqual(bank.spread_attention("src"), 0.0)

        def test_starvation_prevention(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            starved = bank.add_atom("starved")
            starved.sti = 0.0
            rescued = bank.prevent_starvation()
            self.assertEqual(rescued, 1)
            self.assertFalse(starved.is_starved())

        def test_decay_returns_attention_to_funds(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            atom = bank.add_atom("a")
            atom.sti = 100.0
            funds_before = bank.sti_funds
            bank.apply_decay(cycles=10.0)
            self.assertLess(atom.sti, 100.0)
            self.assertGreater(bank.sti_funds, funds_before)

        def test_vlti_promotion(self) -> None:
            bank = AttentionBank(1000.0, 100000.0)
            atom = bank.add_atom("persistent")
            atom.lti = 150.0
            atom.activity_level = 1.0
            bank.pay_wages()
            self.assertGreater(atom.vlti, 0.0)
            self.assertLessEqual(atom.lti, atom.vlti_threshold)

        def test_competition_favors_strong_atoms(self) -> None:
            bank = AttentionBank(1000.0, 1000.0)
            strong = bank.add_atom("strong")
            weak = bank.add_atom("weak")
            strong.competition_strength = 2.0
            strong.activity_level = 0.9
            weak.competition_strength = 0.5
            weak.activity_level = 0.1
            awards = bank.compete_for_attention(100.0)
            self.assertGreater(awards["strong"], awards["weak"])

        def test_scheduler_priority_ordering(self) -> None:
            order: List[str] = []
            sched = AttentionScheduler(max_concurrent_tasks=10, attention_pool=100.0)
            sched.submit_task("low", TaskPriority.LOW, 1.0,
                              action=lambda: order.append("low"))
            sched.submit_task("emergency", TaskPriority.EMERGENCY, 1.0,
                              action=lambda: order.append("emergency"))
            sched.submit_task("normal", TaskPriority.NORMAL, 1.0,
                              action=lambda: order.append("normal"))
            sched.process_tasks(100.0)
            self.assertEqual(order, ["emergency", "normal", "low"])

        def test_scheduler_respects_attention_budget(self) -> None:
            sched = AttentionScheduler(max_concurrent_tasks=10, attention_pool=100.0)
            sched.submit_task("big", TaskPriority.HIGH, 60.0)
            sched.submit_task("small", TaskPriority.LOW, 5.0)
            processed = sched.process_tasks(available_attention=50.0)
            # Big task deferred (over budget) but small one still runs
            self.assertEqual(processed, 1)
            self.assertEqual(sched.stats()["pending_tasks"], 1.0)

        def test_scheduler_cancel(self) -> None:
            sched = AttentionScheduler()
            task_id = sched.submit_task("t", TaskPriority.NORMAL, 1.0)
            self.assertTrue(sched.cancel_task(task_id))
            self.assertEqual(sched.process_tasks(100.0), 0)

        def test_mesh_propagation_and_capacity(self) -> None:
            mesh = AttentionMesh()
            mesh.add_node("a", 50.0)
            mesh.add_node("b", 50.0)
            mesh.connect_nodes("a", "b", 1.0)
            absorbed = mesh.propagate_attention("a", 80.0, propagation_depth=1)
            self.assertGreater(absorbed, 50.0)  # overflow spilled to b
            self.assertLessEqual(mesh.nodes["a"].current_attention, 50.0)

        def test_mesh_load_balancing(self) -> None:
            mesh = AttentionMesh()
            mesh.add_node("hot", 100.0)
            mesh.add_node("cold", 100.0)
            mesh.connect_nodes("hot", "cold", 1.0)
            mesh.nodes["hot"].current_attention = 95.0
            transfers = mesh.balance_load(0.8)
            self.assertGreaterEqual(transfers, 1)
            self.assertLessEqual(mesh.nodes["hot"].utilization, 0.8 + 1e-9)

        def test_mesh_topology_stats(self) -> None:
            mesh = AttentionMesh()
            mesh.add_node("a", 100.0)
            mesh.add_node("b", 200.0)
            mesh.connect_nodes("a", "b", 0.5)
            stats = mesh.topology_stats()
            self.assertEqual(stats["total_nodes"], 2.0)
            self.assertEqual(stats["total_connections"], 1.0)
            self.assertAlmostEqual(stats["avg_node_capacity"], 150.0)

        def test_high_concurrency_stress(self) -> None:
            sched = AttentionScheduler(max_concurrent_tasks=10000, attention_pool=1e9)
            priorities = list(TaskPriority)
            for i in range(10000):
                sched.submit_task("stress", priorities[i % len(priorities)], 1.0)
            self.assertEqual(sched.process_tasks(1e9), 10000)
            self.assertEqual(sched.stats()["scheduler_efficiency"], 1.0)

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(EcanTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Phase 2 ECAN attention allocation resource kernel")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--demo", action="store_true", help="run demonstration")
    mode.add_argument("--benchmark", action="store_true", help="run benchmarks")
    mode.add_argument("--selftest", action="store_true", help="run unit tests")
    args = parser.parse_args(argv)
    if args.benchmark:
        run_benchmark()
        return 0
    if args.selftest:
        return run_selftest()
    run_demo()
    return 0


if __name__ == "__main__":
    sys.exit(main())
