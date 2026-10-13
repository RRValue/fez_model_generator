for m in bpy.data.materials:
    if(m.node_tree is not None):
        if("Image Texture" in m.node_tree.nodes):
            m.node_tree.nodes["Image Texture"].interpolation = "Closest"
        if("Image Texture.001" in m.node_tree.nodes):
            m.node_tree.nodes["Image Texture.001"].interpolation = "Closest"