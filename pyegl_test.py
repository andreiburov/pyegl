# import pdb; pdb.set_trace()
import torch
import pyegl
import trimesh


mesh = trimesh.load('data/bunny_col.obj', process=False)
vertices = torch.tensor((mesh.vertices - mesh.centroid) / mesh.extents, dtype=torch.float32)
colors = torch.tensor(mesh.visual.to_color().vertex_colors / 255.0, dtype=torch.float32)
uv = torch.tensor(mesh.visual.uv, dtype=torch.float32)
vertices_data = torch.cat((vertices, colors, uv), dim=-1)
faces = torch.tensor(mesh.faces, dtype=torch.int64)
n_vertices = vertices_data.shape[0]
n_faces = faces.shape[0]
fx, fy, cx, cy, near, far = 4.14423, 4.27728, 0.5, 0.5, 0.1, 10.0
intrinsics = [fx, fy, cx, cy, near, far]
pose = [1., 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -10, 0, 0, 0, 1]
width, height = 512, 512


pyegl.init(width, height)
maps = pyegl.forward(intrinsics, pose, vertices_data, n_vertices, faces, n_faces)
maps = pyegl.forward(intrinsics, pose, vertices_data, n_vertices, faces, n_faces)