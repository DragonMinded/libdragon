############################################################
# Main magma ucode overlay
#
# Authors: Dennis Heinze <dennis.heinze@mailbox.org>
#
############################################################
#include <rsp_magma.inc>

_MgDefineUniformRegion

_MgBeginShader
    # Bootstrap shader code
    # This code is only executed when the magma ucode has just been switched to.
    # Otherwise, if magma is already loaded, the shader code replaces this one.
    # In the latter case rspq will directly jump into the shader code when loading vertices.
    _MgBootstrapShader
MgEndShader
