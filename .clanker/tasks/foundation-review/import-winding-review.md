# Import and winding correction source review

Reviewed current stalthos-runtime/engine/main.cpp against the local pinned Wicked source, without GPU launch.

- ModelImporter_GLTF.cpp:209-240 indexes mesh managers directly with glTF node.mesh; import into fresh Scene keeps the mesh index origin at zero.
- ModelImporter_GLTF.cpp:345-428 FlipZAxis loops every mesh and transform in the supplied Scene. Fresh Scene prevents terrain/rocks from being mirrored or recull-flipped.
- ModelImporter_GLTF.cpp:602-604 creates a root transform; 1667 loads scene nodes beneath that root. Host translates transforms lacking a hierarchy component, which translates the import root once and leaves child local transforms intact.
- wiScene_Components.cpp:147-152 Translate adds to translation_local and marks dirty. Host then calls UpdateTransform; the subsequent engine Scene update propagates hierarchy.
- wiScene.cpp:1158-1171 Merge transfers component managers, so imported entity links remain references to transferred entity identities.
- wiScene_Components.cpp:1487-1490 computes normal cross(p2-p0,p1-p0). Corrected host triangles a,b,c and b,d,c have positive Y cross component 4 on the 2-unit X/Z lattice, independently of Ground height. Earlier a,c,b had negative Y.

Conclusion: concrete importer isolation/top-level translation and winding edits agree with local engine semantics. No new source-level defects found. Actual imported appearance remains root GPU rerender evidence, not established by this review.
