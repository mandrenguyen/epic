#!/bin/bash
# setup_dd4hep.sh
# Quickly set up Python 3.12, ROOT, and DD4hep in the container

# --- Step 1: Use the container's Python 3.12
export PATH=/opt/software/linux-x86_64_v2/python-3.12.9-vn2lg3p7qrihanno4s75w5oxrppm5zc7/bin:$PATH

# --- Step 2: Load ROOT environment
source /opt/software/linux-x86_64_v2/root-6.36.02-zccki6dan46nwndn2bfxfvotpj3b6ngl/bin/thisroot.sh

# --- Step 3: Set DD4hep Python bindings
export PYTHONPATH=/opt/software/linux-x86_64_v2/dd4hep-1.32.1-dmqlofg74smimvtprblln4actoz36te5/lib/python3.12/site-packages:$PYTHONPATH

# --- Step 4: Confirm versions (optional)
echo "Python version: $(python3 --version)"
echo "Pip version: $(pip3 --version)"
python3 -c "import dd4hep, DDG4; print('✅ dd4hep and DDG4 imported successfully')"
