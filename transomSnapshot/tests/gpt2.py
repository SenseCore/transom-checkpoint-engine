import time

import torch
from transformers import GPT2Model, GPT2Tokenizer

from transomSnapshot.engine import engine

model_name = "gpt2-large"
tokenizer = GPT2Tokenizer.from_pretrained(model_name)
model = GPT2Model.from_pretrained(model_name)
device = torch.device("cuda", 0) if torch.cuda.is_available() else torch.device("cpu")
model.to(device)
start = time.perf_counter()
engine.save(model, "1" + model_name)
# engine.save(model, "2" + model_name)
# engine.save(model, "3" + model_name)
# engine.save(model, "4" + model_name)
elapsed = time.perf_counter() - start
print(f"save {elapsed:0.4f} seconds.")
