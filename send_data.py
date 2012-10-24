import bpy
import struct
import binascii
from mathutils import Matrix, Vector

device = "serial"

vector_data = []

camera = None
objects = bpy.data.objects
for object in objects:
	if object.type == "CAMERA":
		camera = object
	if object.type != "MESH":
		continue
	matrix_world = object.matrix_world
	mesh = object.to_mesh(bpy.context.scene, False, settings = 'PREVIEW')
	vertices = list(mesh.vertices)
	for edge in mesh.edges:
		for vertice_idx in edge.vertices:
			v = vertices[vertice_idx].co * matrix_world
			vector = struct.pack(">3f", v.x, v.y, v.z)
			vector_data.append(vector + struct.pack(">L", binascii.crc32(vector)))

vector_data = b"".join(vector_data)
edge_count = len(vector_data) >> 5 # 2^4 - vector size, 2^5 - edge size
vector_data = struct.pack(">H", edge_count & 0x7fff) + b"\x00\x00" + vector_data

w = 1920
h = 1080
ratio = w / h
RT = camera.matrix_world.inverted()
C = Matrix().to_4x4()
C[0][0] = (2.0 * 1000) / w
C[1][1] = (2.0 * 1000) / h
C[2][2] = (-(1000 + 10000)) / (10000 - 1000)
C[2][3] = (-2.0 * 10000 * 1000) / (10000 - 1000)
C[3][2] = -1.0
C[3][3] = 0.0

S = Matrix().to_4x4()
S[0][0] = w
S[1][1] = h
S[0][3] = w / 2
S[1][3] = h / 2

v = Vector((1.0, 1.0, 1.0, 1.0))

TM = C * S * RT
#tv = (S * C * RT) * v
#print((1/tv[3]) * tv)

camera_data = b"\x10\x00\x00\x00"
for i in range(8):
	row = i >> 1
	if i % 2 == 0:
		data = struct.pack(">3f", TM[row][0], TM[row][1], 0.0)
	else:
		data = struct.pack(">3f", TM[row][2], TM[row][3], 0.0)
	camera_data += (data + struct.pack(">L", binascii.crc32(data)))

f = open(device, "wb")
f.write(camera_data + vector_data)
f.close()
