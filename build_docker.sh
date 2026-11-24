#! /bin/bash
set -eoux pipefail

BASE_IMAGE=$1

docker build \
	--build-arg=BASE_IMAGE="${BASE_IMAGE}" \
	--tag supautils_rocky \
	.

container_id=$(
	docker container create \
		supautils_rocky:latest
)

docker cp \
	./ \
	"${container_id}":/home/postgres/supautils

docker start \
	--attach "${container_id}"
