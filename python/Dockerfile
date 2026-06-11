FROM python:3.12 AS python_env
COPY requirements.txt /tmp/requirements.txt
RUN python -m pip install --upgrade pip
RUN python -m pip install --no-cache-dir -r /tmp/requirements.txt
COPY . /africapolis/
WORKDIR /africapolis