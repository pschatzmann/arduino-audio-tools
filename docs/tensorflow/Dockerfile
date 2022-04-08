FROM snowzach/tensorflow-multiarch
#FROM tensorflow/tensorflow:latest-jupyter
LABEL maintainer="phil schatzmann"
RUN /usr/bin/python3 -m pip install --upgrade pip
RUN pip3 install seaborn matplotlib jupyterlab

# Working directory
RUN mkdir /content
WORKDIR /content
VOLUME /content
ADD micro_speech_with_lstm_op.ipynb /content
ADD train_micro_speech_model.ipynb /content
ADD train_hello_world_model.ipynb /content
RUN apt-get update && apt-get install git -y && apt-get upgrade -y
EXPOSE 8888
CMD ["jupyter", "lab", "--no-browser", "--allow-root", "--ip=0.0.0.0" ]
