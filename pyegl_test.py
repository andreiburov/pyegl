# import pdb; pdb.set_trace()
import torch
import pyegl
import trimesh
import imageio
import numpy as np

mesh = trimesh.load('data/bunny_col.obj', process=False)
mesh.apply_translation(-mesh.centroid).apply_scale(1./mesh.extents)
n_vertices = mesh.vertices.shape[0]
n_faces = mesh.faces.shape[0]
vertices = torch.tensor(mesh.vertices, dtype=torch.float32)
normals = torch.tensor(mesh.vertex_normals.copy(), dtype=torch.float32)
# colors = torch.tensor(mesh.visual.to_color().vertex_colors / 255.0, dtype=torch.float32)
colors = torch.ones((n_vertices, 4), dtype=torch.float32)
uv = torch.tensor(mesh.visual.uv, dtype=torch.float32)
mask = torch.ones((n_vertices, 1), dtype=torch.float32)
vertices_data = torch.cat((vertices, normals, colors, uv, mask), dim=-1).cuda()
faces = torch.tensor(mesh.faces, dtype=torch.int64)


fx, fy, cx, cy, near, far = 1000, 1000, 256, 256, 0.01, 100.0
intrinsics = [fx, fy, cx, cy, near, far] # + [0]
pose = [1., 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 3, 0, 0, 0, 1]
width, height = 512, 512


pyegl.init(width, height)
#pyegl.init_with_defines(width, height, ['CONSTANT_SHADING'])
#pyegl.init_with_defines(width, height, ['DIFFUSE_SHADING'])
pyegl.init_with_defines(width, height, ['TEXTURE_SHADING'])
pyegl.load_config('data/config.json')
pyegl.load_shader(['TEXTURE_SHADING', 'DIFFUSE_SHADING'])
pyegl.attach_texture('data/bunny-atlas.jpg')
maps = pyegl.forward(intrinsics, pose, vertices_data, n_vertices, faces, n_faces)

color = maps[0].flip([0]).cpu()
color = torch.where(color[..., 0:3] == -1, torch.zeros_like(color[..., 0:3]), color[..., 0:3]).numpy()
color = (color * 255).astype(np.uint8)
np.clip(color, 0, 255, out=color)
imageio.imwrite('results/color.png', color)

positions = maps[1].flip([0]).cpu().numpy()[..., 0:3]
positions = ((positions * 0.5 + 0.5) * 255).astype(np.uint8)
np.clip(positions, 0, 255, out=positions)
imageio.imwrite('results/positions.png', positions)

normals = maps[2].flip([0]).cpu().numpy()[..., 0:3]
normals = normals * 0.5 + 0.5
normals = (normals * 255).astype(np.uint8)
np.clip(normals, 0, 255, out=normals)
imageio.imwrite('results/normals.png', normals)

uv = maps[3].flip([0]).cpu()
uv = torch.where(uv == -1, torch.zeros_like(uv), uv)
uv = torch.cat((uv, torch.zeros((height, width, 1), dtype=torch.float32)), dim=-1).numpy()
uv = (uv * 255).astype(np.uint8)
np.clip(uv, 0, 255, out=uv)
imageio.imwrite('results/uv.png', uv)

barycentrics = maps[4].flip([0]).cpu().numpy()
barycentrics = (barycentrics * 255).astype(np.uint8)
np.clip(barycentrics, 0, 255, out=barycentrics)
imageio.imwrite('results/barycentrics.png', barycentrics)

vertex_ids = (maps[5].flip([0])[..., 0:3]/maps[5].max()).cpu().numpy()
vertex_ids = (vertex_ids * 255).astype(np.uint8)
np.clip(vertex_ids, 0, 255, out=vertex_ids)
imageio.imwrite('results/vertex_ids.png', vertex_ids)

pyegl.terminate()
