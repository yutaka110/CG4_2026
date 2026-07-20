# GE3 Shipping Runtime Third-Party Notices

The GE3 Shipping Runtime target does not compile or link the repository's vendored
Assimp, DirectXTex, or Dear ImGui source and binary packages. It uses Windows and
Direct3D system APIs supplied by the target operating system.

This notice applies only to the minimal `GE3.Runtime` Shipping target. Products
that add runtime middleware must update this notice and the dependency lock before
distribution.
