import importlib,json
import pytest
from stalthos_toolkit.cli import main
MOD=importlib.import_module('stalthos_toolkit.commands.generate_plant')

def run(capsys,tmp_path,monkeypatch,d,stage='sapling'):
 monkeypatch.setattr(MOD.Context,'load_definition',lambda ctx,path:d)
 args=['generate_plant','-d','fixture.yaml','--workspace',str(tmp_path),'--dry-run']
 if stage: args+=['--stage',stage]
 code=main(args);c=capsys.readouterr()
 assert 'Traceback' not in c.out+c.err
 return code,json.loads(c.out)

def tree():
 return {'id':'plant.synthetic','kind':'plant','generator':'branching_tree','height_m':10.,'growth_stages':{'sapling':{'height_m':2.}}}

@pytest.mark.parametrize('alias',['growth_stages','life_stages','stages'])
@pytest.mark.parametrize('shape',['list','mapping','leaf_scale'])
def test_stage_aliases(capsys,tmp_path,monkeypatch,alias,shape):
 d=tree();d.pop('growth_stages')
 if shape=='list':
  d[alias]=[{'id':'sapling','height':{'value':2,'unit':'m'}},{'id':'adult','height':{'value':10,'unit':'m'}}];expected=.18
 elif shape=='mapping':
  d[alias]={'sapling':{'height_m':2.,'trunk_diameter_m':.12,'leaf_length_m':.08},'adult':{'height_m':10.}};expected=.08
 else:
  d['leaf_length_m']=.4;d[alias]={'sapling':{'height_m':2.,'leaf_scale':.5}};expected=.2
 code,s=run(capsys,tmp_path,monkeypatch,d)
 assert code==0 and s['ok']
 p=s['metrics']['plan']['plant']
 assert p['height_m']==pytest.approx(2.)
 assert p['leaf_length_m']==pytest.approx(expected)
 if shape=='mapping':assert p['trunk_diameter_m']==pytest.approx(.12)

@pytest.mark.parametrize('morph',[{'organ':'scale_spray'},{'overall_shape':'needled'}])
@pytest.mark.parametrize('dimension',['length','width'])
@pytest.mark.parametrize('invalid',[None,0,-1,float('nan'),float('inf'),float('-inf')])
def test_invalid_needle_status(capsys,tmp_path,monkeypatch,morph,dimension,invalid):
 d=tree();f=dict(morph,length={'value':2,'unit':'cm'},width={'value':1,'unit':'mm'})
 if invalid is None:f.pop(dimension)
 else:f[dimension]={'value':invalid,'unit':'cm'}
 d['anatomy']={'foliage':f}
 code,s=run(capsys,tmp_path,monkeypatch,d)
 assert code==s['exit_code']==2
 assert not s['ok'] and not s['artifacts']

def test_units_and_no_stage(capsys,tmp_path,monkeypatch):
 d=tree();d['anatomy']={'foliage':{'organ':'scale_spray','length':{'value':2,'unit':'cm'},'width':{'value':1,'unit':'mm'}}}
 code,s=run(capsys,tmp_path,monkeypatch,d)
 assert code==0
 p=s['metrics']['plan']['plant']
 assert p['needle_length_m']==pytest.approx(.02)
 assert p['needle_width_m']==pytest.approx(.001)
 code,s=run(capsys,tmp_path,monkeypatch,d,stage=None)
 assert code==0 and 'needle_length_m' not in s['metrics']['plan']['plant']
