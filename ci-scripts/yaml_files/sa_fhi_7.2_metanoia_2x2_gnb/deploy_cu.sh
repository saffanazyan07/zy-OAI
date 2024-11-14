set -e
helm install --wait --timeout 60s oai-cu /opt/oai-cn5g-fed-develop-2024-april-20897/ci-scripts/charts/oai-cu
exit 0
