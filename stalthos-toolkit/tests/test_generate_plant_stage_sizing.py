import importlib,json
from pathlib import Path
import pytest
from stalthos_toolkit.cli import main
from stalthos_toolkit.blender.params import plant_shape_params
import yaml

MOD=importlib.import_module('stalthos_toolkit.commands.generate_plant')
DEFINITION=Path(__file__).resolve().parents[1]/'species/plants/splitcrown.yaml'

def run(capsys,tmp_path,stage=None,dry=True):
 args=['generate_plant','-d',str(DEFINITION),'-s','42','--workspace',str(tmp_path)]
 if stage:args+=['--stage',stage]
 if dry:args+=['--dry-run']
 code=main(args);envelope=json.loads(capsys.readouterr().out)
 return code,envelope

@pytest.mark.parametrize('stage,height',[('sapling',4.2),('young_adult',14.0),('mature',26.0)])
def test_real_definition_stage_plan(capsys,tmp_path,stage,height):
 code,s=run(capsys,tmp_path,stage)
 assert code==0 and s['ok']
 assert s['metrics']['plan']['plant']['height_m']==pytest.approx(height)
 assert not s['artifacts']

def test_no_stage_shape_unchanged(capsys,tmp_path):
 code,s=run(capsys,tmp_path)
 definition=yaml.safe_load(DEFINITION.read_text())
 assert code==0 and s['metrics']['plan']['plant']==plant_shape_params(definition,'tree',stage=None)

def test_invalid_stage_status(capsys,tmp_path):
 code,s=run(capsys,tmp_path,'not-a-stage')
 assert code==2 and not s['ok'] and not s['artifacts']

@pytest.mark.parametrize('stage,height',[('sapling',4.2),('young_adult',14.0)])
def test_script_payload_matches_plan(capsys,tmp_path,monkeypatch,stage,height):
 seen={}
 monkeypatch.setattr(MOD.PL,'require_blender',lambda ctx:None)
 def script(ctx,status,**kwargs):
  seen.update(kwargs['params'])
  out=Path(kwargs['params']['outputs']['blend']);out.write_bytes(b'explicit unit fixture, not a Blender master')
  return None,{'artifacts':[{'path':str(out),'role':'source','kind':'blend'}],'metrics':{}}
 monkeypatch.setattr(MOD.PL,'run_script',script)
 code,s=run(capsys,tmp_path,stage,dry=False)
 assert code==0 and s['ok']
 assert seen['plant']['height_m']==pytest.approx(height)
 assert seen['growth_stage']['available']


def test_legacy_absolute_stage_dimensions(capsys,tmp_path,monkeypatch):
    definition={'id':'plant.synthetic.legacy','kind':'plant','generator':'branching_tree',
                'height_m':10.0,'growth_stages':{
                    'sapling':{'height_m':2.0,'trunk_diameter_m':0.2,'leaf_length_m':0.08},
                    'mature':{'height_m':10.0}}}
    monkeypatch.setattr(MOD.Context,'load_definition',lambda ctx,path:definition)
    code,s=run(capsys,tmp_path,'sapling')
    shape=s['metrics']['plan']['plant']
    assert code==0 and shape['height_m']==pytest.approx(2.0)
    assert shape['trunk_diameter_m']==pytest.approx(0.2)
    assert shape['leaf_length_m']==pytest.approx(0.08)


def test_declared_stage_height_overrides_base_height(capsys,tmp_path,monkeypatch):
    definition=yaml.safe_load(DEFINITION.read_text());definition['height_m']=10.0
    monkeypatch.setattr(MOD.Context,'load_definition',lambda ctx,path:definition)
    code,s=run(capsys,tmp_path,'young_adult')
    assert code==0 and s['metrics']['plan']['plant']['height_m']==pytest.approx(14.0)
