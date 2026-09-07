import torch
import torch.nn as nn


class EyeCNN(nn.Module):
    def __init__(self):
        super(EyeCNN, self).__init__()

        # 第一层卷积
        self.conv1 = nn.Sequential(
            nn.Conv2d(
                in_channels=3,
                out_channels=16,
                kernel_size=3,
                stride=1,
                padding=1
            ),
            nn.ReLU(),
            nn.MaxPool2d(2)
        )

        # 第二层卷积
        self.conv2 = nn.Sequential(
            nn.Conv2d(
                in_channels=16,
                out_channels=32,
                kernel_size=3,
                stride=1,
                padding=1
            ),
            nn.ReLU(),
            nn.MaxPool2d(2)
        )


        # 分类层
        self.fc = nn.Sequential(
            nn.Linear(32 * 56 * 56, 128),
            nn.ReLU(),
            nn.Linear(128, 2)
        )


    def forward(self, x):

        x = self.conv1(x)

        x = self.conv2(x)

        x = x.view(x.size(0), -1)

        x = self.fc(x)

        return x