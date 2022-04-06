FROM tensorflow/tensorflow:latest-jupyter
LABEL maintainer="phil schatzmann"
RUN /usr/bin/python3 -m pip install --upgrade pip
RUN pip3 install seaborn matplotlib

# Working directory
RUN mkdir /home/tensorflow
WORKDIR /home/tensorflow
VOLUME /home/tensorflow
ADD micro_speech_with_lstm_op.ipynb /home/tensorflow
EXPOSE 8888
CMD ["jupyter", "lab", "--no-browser", "--allow-root", "--ip=0.0.0.0" ]
