# OS_Homework2

실습자료로 주어진 프로그램에서 4GB 이상의 파일을 생성후 mmio를 이용해 파일 복사를 시도하면 

MapViewOfFile()에서 에러코드를 출력한다. 

해결방법으로 1기가와 같이 핸들링 가능한 단위로 메모리맵을 생성해 

여러번 핸들링 해주어 파일 처리를 하려고 했으나 

1GB 단위로 처리시 4.5기가 파일에서 나머지 500MB에 대한 오프셋 처리가 미숙하여 

멘토님이 주신 FileHelperClass를 사용해서 다음과 같이 코딩하였다. 

![alt tag](https://github.com/morinori/OS_Homework2/HW1.png)
