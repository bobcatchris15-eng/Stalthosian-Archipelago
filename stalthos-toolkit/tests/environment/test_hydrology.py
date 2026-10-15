import pytest
from stalthos_toolkit.environment.heightfield import Heightfield
from stalthos_toolkit.environment.hydrology import FlowRouting, MoistureField, HydrologyResult, summarize, analyze, WaterTableSpec
from stalthos_toolkit.core.contract import Context
from stalthos_toolkit.core.workspace import Workspace
from pathlib import Path

def test_summarize_empty():
    hf = Heightfield(width=0, height=0, cell_size=1.0, sea_level=0.0)
    res = HydrologyResult(
        routing=FlowRouting(
            downhill=[],
            accumulation=[],
            channel_mask=[],
            filled_elevation=[],
            sea_level=0.0,
            cell_size=1.0,
            filled_cells=0,
            max_fill_depth=0.0,
            lake_cells=[]
        ),
        moisture=MoistureField(
            suitability=[],
            water_table=[],
            mean_suitability=0.0,
            wet_cell_fraction=0.0,
            mean_permeability_offset=0.0
        ),
        drainage_density=0.0,
        dominated_by_rivers=False
    )

    metrics = summarize(hf, res)
    assert metrics["cells"] == 0
    assert metrics["drainage_density"] == 0.0
    assert metrics["dominated_by_rivers"] is False
    assert metrics["channel_cell_fraction"] == 0.0
    assert metrics["lake_cells"] == 0
    assert metrics["max_accumulation"] == 0.0

def test_summarize_populated():
    hf = Heightfield(width=2, height=2, cell_size=1.0, sea_level=0.0)
    hf.data = [0.0, 1.0, 2.0, 3.0]

    # 4 cells, 1 channel cell
    res = HydrologyResult(
        routing=FlowRouting(
            downhill=[1, 2, 3, -1],
            accumulation=[1.0, 2.0, 3.0, 4.0],
            channel_mask=[False, False, False, True], # 1 channel cell
            filled_elevation=[0.0, 1.0, 2.0, 3.0],
            sea_level=0.0,
            cell_size=1.0,
            filled_cells=1,
            max_fill_depth=2.5,
            lake_cells=[3] # 1 lake cell
        ),
        moisture=MoistureField(
            suitability=[0.1, 0.2, 0.3, 0.9],
            water_table=[0.0, 0.0, 0.0, 0.0],
            mean_suitability=0.375,
            wet_cell_fraction=0.25,
            mean_permeability_offset=0.1
        ),
        drainage_density=0.25,
        dominated_by_rivers=True
    )

    metrics = summarize(hf, res)
    assert metrics["cells"] == 4
    assert metrics["drainage_density"] == 0.25
    assert metrics["dominated_by_rivers"] is True
    assert metrics["channel_cell_fraction"] == 0.25
    assert metrics["channel_cells"] == 1
    assert metrics["lake_cells"] == 1
    assert metrics["max_accumulation"] == 4.0
    assert metrics["depression_filled_cells"] == 1
    assert metrics["max_depression_fill"] == 2.5
    assert metrics["mean_moisture_suitability"] == 0.375
    assert metrics["wet_cell_fraction"] == 0.25

def test_route_flow_depression():
    from stalthos_toolkit.environment.hydrology import route_flow
    hf = Heightfield(width=3, height=3, cell_size=1.0, sea_level=0.0)
    # A bowl shape: center is lower than edges
    hf.data = [2.0, 2.0, 2.0,
               2.0, 1.0, 2.0,
               2.0, 2.0, 2.0]

    routing = route_flow(hf, sea_level=0.0, stream_threshold=2, floodplain_cells=0)

    # The center cell (idx 4) should be filled to escape the depression
    assert routing.filled_cells == 1
    assert routing.max_fill_depth > 0.0
    # Center cell flows outward to its lowest neighbor (or one chosen deterministically)
    assert routing.downhill[4] != -1

def test_moisture_suitability():
    from stalthos_toolkit.environment.hydrology import moisture_suitability, WaterTableSpec

    hf = Heightfield(width=2, height=2, cell_size=1.0, sea_level=0.0)
    hf.data = [0.0, 1.0, 2.0, 3.0]

    routing = FlowRouting(
        downhill=[1, 2, 3, -1],
        accumulation=[1.0, 2.0, 3.0, 4.0],
        channel_mask=[False, False, False, True],
        filled_elevation=[0.0, 1.0, 2.0, 3.0],
        sea_level=0.0,
        cell_size=1.0,
        filled_cells=0,
        max_fill_depth=0.0,
        lake_cells=[]
    )

    spec = WaterTableSpec(base_level=0.0, gradient=0.1, permeability_jitter=0.0)
    ctx = Context(workspace=Workspace(Path("/tmp/stalthos-test")), seed=42)

    moisture = moisture_suitability(hf, routing, spec, ctx)

    assert len(moisture.suitability) == 4
    assert len(moisture.water_table) == 4
    assert not moisture.water_table_metric_measured
    # Standing water (accumulation <= 1.0 and elevation <= sea level) gets wettest=1.0
    assert moisture.suitability[0] == 1.0

def test_analyze_orchestration():
    hf = Heightfield(width=3, height=3, cell_size=1.0, sea_level=0.0)
    hf.data = [
        3.0, 3.0, 3.0,
        2.0, 2.0, 2.0,
        1.0, 1.0, 1.0
    ] # simple slope

    ctx = Context(workspace=Workspace(Path("/tmp/stalthos-test")), seed=123)
    spec = WaterTableSpec(stream_threshold=2)

    result = analyze(hf, sea_level=0.0, spec=spec, ctx=ctx)

    assert isinstance(result, HydrologyResult)
    assert result.routing.channel_cell_count() > 0
    assert len(result.moisture.suitability) == 9
    assert result.drainage_density > 0.0

def test_floodplain_widening():
    from stalthos_toolkit.environment.hydrology import route_flow

    hf = Heightfield(width=5, height=5, cell_size=1.0, sea_level=0.0)
    # A V-shaped valley to create a single central stream
    hf.data = [
        4.0, 3.0, 2.0, 3.0, 4.0,
        4.0, 3.0, 1.5, 3.0, 4.0,
        4.0, 3.0, 1.0, 3.0, 4.0,
        4.0, 3.0, 0.5, 3.0, 4.0,
        4.0, 3.0, 0.0, 3.0, 4.0
    ]

    routing_no_floodplain = route_flow(hf, sea_level=-1.0, stream_threshold=3, floodplain_cells=0)
    routing_floodplain = route_flow(hf, sea_level=-1.0, stream_threshold=3, floodplain_cells=1)

    channels_base = sum(routing_no_floodplain.channel_mask)
    channels_wide = sum(routing_floodplain.channel_mask)

    assert channels_wide > channels_base
