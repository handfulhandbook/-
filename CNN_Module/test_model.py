import torch
from model import EyeCNN


model = EyeCNN()

x = torch.randn(1,3,224,224)

output = model(x)

print(output)