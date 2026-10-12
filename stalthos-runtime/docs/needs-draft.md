# Needs Draft Rules

This document outlines the proposed full draft needs system for the 5 Hero Beasts in The Stalthosian Archipelago.

## Needs Profile and Urgency

The needs are represented by four components, normalized between `0.0` (optimal) and `1.0` (critical):
- **Hunger**
- **Thirst**
- **Fatigue**
- **Injury**

Base rates are proposed as tuning defaults. Currently, all five species reuse provisional rates. The numbers are explicitly fiction-first and not intended to represent accurate scientific physiology.

## Advancement and Responsiveness Policy

Progression is managed by `TimeStep`, comprising `calendarSeconds` and `physicalSeconds`.

**Explicit Responsiveness Policy:**
- Calendar time explicitly drives slow-acting metabolic needs: Hunger and Thirst.
- Physical time drives immediate physical capacities and reflexes: Fatigue.
- Fast-forwarding the calendar (for example, during rest or nesting) accelerated hunger and thirst seamlessly. It does NOT multiply physical reflexes or resting recovery a second time, avoiding doubly applying physical interventions.

Time steps with NaN or negative values are rejected. Arithmetic overflow is rejected before any state is changed; large finite steps whose calculations remain finite are accepted and saturate at the normalized bounds.

## Interventions

Interventions (`feed`, `drink`, `rest`, `sufferInjury`, `healInjury`) are bounded and strictly clamped between `0.0` and `1.0`. They act as safe mutators.

## Activity Suggestions

The system provides a `suggestActivity` method that evaluates the most urgent need exceeding a given threshold.
- The resulting suggestion is derived context.
- **It is explicitly not an authority or a secondary controller.** The suggestion itself does not forcefully mutate the state, invoke actions, or mandate autonomous behavior. It operates only to aid decision systems (e.g. Host controllers) in choosing activities.

## Explicitly Unfinished Features

The following aspects are explicitly missing and are subject to future integration:
- **Host Integration:** Feeding and navigation, as well as integrating need suggestions into real autonomous goals.
- **Resource Perception/Contact:** Finding and depleting real flora and water sources.
- **Persistence:** Storing need values within save components.
- **Ecology/Social Behavior:** Incorporating herd dynamics or predator-prey actions.
- **Terminal Lifecycle Death Link:** Reaching critical needs levels (`1.0`) currently does NOT forcefully kill the entity by interacting with the `Lifecycle` state machine. This linkage remains pending.
- **Final Tuning:** The profile variables applied are provisional defaults for all five species. They are expected to be uniquely tuned per species upon final implementation.
