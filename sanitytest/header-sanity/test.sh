#!/bin/sh
set -xe 
HEADERS_DIRECTORY=$(readlink -f ../../include)
mapfile -t -d '' ARRAY_H_HH < <(find ${HEADERS_DIRECTORY} -type f \
    \( -iname \*.h -o -iname \*.hh \) -print0 | \
    xargs -0 -n1 -I '{}' realpath -e -z '{}')

for HEADER in "${ARRAY_H_HH[@]}"
do
    case ${HEADER} in
    */rtapi_app.h) continue ;;
    */rtapi_common.h) continue ;;
    */*.pb.h) continue ;;
    */container.h) continue ;;
    esac
    if [ "${HEADER: -2}" == ".h" ]
    then
        gcc -DULAPI -I${HEADERS_DIRECTORY} -E -x c ${HEADER} > /dev/null
    fi
    g++ -DULAPI -I${HEADERS_DIRECTORY} -E -x c++ ${HEADER} > /dev/null
done
