FROM ortools/cmake:opensuse_swig AS env

ENV PATH=/root/.local/bin:$PATH
RUN zypper refresh \
&& zypper install -y python311 python311-devel \
 python311-pip python311-wheel python311-virtualenv python311-setuptools \
&& zypper clean -a
RUN python3.11 -m pip install --break-system-packages \
 absl-py mypy mypy-protobuf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN cmake -S. -Bbuild -DBUILD_PYTHON=ON -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build --target all -v
RUN cmake --build build --target install

FROM build AS test
RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test

FROM env AS install_env
WORKDIR /home/sample
COPY --from=build /home/project/build/python/dist/*.whl .
RUN python3 -m pip install --break-system-packages *.whl

FROM install_env AS install_devel
COPY cmake/samples/python .

FROM install_devel AS install_build
RUN python3 -m compileall .

FROM install_build AS install_test
RUN python3 sample.py
