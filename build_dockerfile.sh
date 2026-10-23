#! /bin/bash
set -eoux pipefail

docker build --file Dockerfile.rocky --tag supautils_rocky .
container_id=$(
	docker container create \
		supautils_rocky:latest
)
docker cp ./ "${container_id}":/home/supautils/supautils
docker start --attach "${container_id}"
