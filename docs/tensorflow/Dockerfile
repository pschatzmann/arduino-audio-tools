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
EXPOSE 8888
CMD ["jupyter", "lab", "--no-browser", "--allow-root", "--ip=0.0.0.0" ]
