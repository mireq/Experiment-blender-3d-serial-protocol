import bpy
import struct
import binascii
import threading
import time
from mathutils import Matrix, Vector

device = "/home/mirec/serial"

def get_vector_data():
	vector_data = []

	objects = bpy.data.objects
	for object in objects:
		if object.type != "MESH":
			continue
		matrix_world = object.matrix_world
		mesh = object.to_mesh(bpy.context.scene, True, settings = 'PREVIEW')
		vertices = list(mesh.vertices)
		for edge in mesh.edges:
			for vertice_idx in edge.vertices:
				v = vertices[vertice_idx].co
				v = matrix_world * Vector((v.x, v.y, v.z, 1.0))
				vector = struct.pack("<3f", v[0], v[1], v[2])
				vector_data.append(vector + struct.pack("<L", binascii.crc32(vector)))

	vector_data = b"".join(vector_data)
	edge_count = len(vector_data) >> 5 # 2^4 - vector size, 2^5 - edge size
	edge_count_pack = struct.pack("<H", edge_count & 0x7fff) + (10 * b"\x00")
	vector_data = edge_count_pack + struct.pack("<L", binascii.crc32(edge_count_pack)) + vector_data
	return vector_data

def get_camera_data():
	camera = bpy.data.objects['Camera']
	w = 1920
	h = 1080
	RT = camera.matrix_world.inverted()
	C = Matrix().to_4x4()
	C[0][0] = (-2.0 * 1000) / w
	C[1][1] = (-2.0 * 1000) / h
	C[2][2] = (-(1000 + 10000)) / (10000 - 1000)
	C[2][3] = (-2.0 * 10000 * 1000) / (10000 - 1000)
	C[3][2] = -1.0
	C[3][3] = 0.0

	S = Matrix().to_4x4()
	S[0][0] = -w
	S[1][1] = h
	S[0][3] = w / 2
	S[1][3] = h / 2

	v = Vector((1.0, 1.0, 1.0, 1.0))

	TM = S * C * RT
	camera_data = b"\x80" + (15 * b"\x00")
	for i in range(8):
		row = i >> 1
		if i % 2 == 0:
			data = struct.pack("<3f", TM[row][0], TM[row][1], 0.0)
		else:
			data = struct.pack("<3f", TM[row][2], TM[row][3], 0.0)
		camera_data += (data + struct.pack("<L", binascii.crc32(data)))

	return camera_data

f = open(device, "wb")
#f.write(camera_data + vector_data)

def write_scene():
	time.sleep(1.0)
	scene_data = get_camera_data() + get_vector_data()
	f.write(scene_data)
	f.flush()
	i = -1
	while True:
		if i % 100 == 0:
			scene_data = get_camera_data() + get_vector_data()
		else:
			scene_data = get_camera_data()
		f.write(scene_data)
		f.flush()
		time.sleep(0.05)
		i += 1

t = threading.Thread(target = write_scene)
t.start()
