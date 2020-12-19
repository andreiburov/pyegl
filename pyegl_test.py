# import pdb; pdb.set_trace()
import torch
import pyegl
import trimesh


mesh = trimesh.load('data/bunny_col.obj', process=False)
mesh.apply_translation(-mesh.centroid).apply_scale(1./mesh.extents)
n_vertices = mesh.vertices.shape[0]
n_faces = mesh.faces.shape[0]
vertices = torch.tensor(mesh.vertices, dtype=torch.float32)
normals = torch.tensor(mesh.vertex_normals.copy(), dtype=torch.float32)
colors = torch.tensor(mesh.visual.to_color().vertex_colors / 255.0, dtype=torch.float32)
uv = torch.tensor(mesh.visual.uv, dtype=torch.float32)
mask = torch.ones((n_vertices, 1), dtype=torch.float32)
vertices_data = torch.cat((vertices, normals, colors, uv, mask), dim=-1).cuda()
faces = torch.tensor(mesh.faces, dtype=torch.int64)
fx, fy, cx, cy, near, far = 4.14423, 4.27728, 0.5, 0.5, 0.1, 10.0
intrinsics = [fx, fy, cx, cy, near, far]
pose = [1., 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -10, 0, 0, 0, 1]
width, height = 512, 512


pyegl.init(width, height)
maps = pyegl.forward(intrinsics, pose, vertices_data, n_vertices, faces, n_faces)
maps = pyegl.forward(intrinsics, pose, vertices_data, n_vertices, faces, n_faces)
pyegl.terminate()